#ifndef __LEWIS__HPP__
#define __LEWIS__HPP__

#include <iostream>
#include <cstdio>
#include <string>
#include <memory>
#include <pthread.h>
#include <sstream>
#include <unistd.h>
#include <map>
#include <unordered_map>
#include <json/json.h>
#include "base/http.h"
#include "speech.h"
using namespace std;

#define BSIZE 128
#define ETC "./command.etc"
#define VOICE_FILE "voice.wav"
#define VOICE_TTS_FILE "tts.mp3"


//服务类
class Util{
private:
    static pthread_t tid;
public:
    //执行命令并判断是否需要打印信息，不需要则将其输出到垃圾桶
    static bool Exec(string command, bool is_print)
    {
	//将指令附属信息重定向至garbage
	if(!is_print){
	    command += ">/dev/null 2>&1";
	}

	//popen(comm, type)函数会创造一个管道，再fork一个子进程
	//在子进程中执行comm命令，然后stdin/stdot文件指针
	FILE *fp = popen(command.c_str(), "r");
	if(NULL == fp){
	    cerr << "popen error" << endl;
	    return false;
	}

	if(is_print){
	    char c;
	    while(fread(&c, 1, 1, fp) > 0){
		cout << c;
	    }
	}
	pclose(fp);
	return true;
    }
  
    static void* Move(void *arg)
    {
	string message = (char*)arg;
	const char *lable = "......";
	const char *blank = "      ";
	const char *x = "|/-\\";
	int i = 4;
	int j = 0;
	while(1){
	    cout << message << "[" << x[j % 3] << "]" << lable + i << "\r";
	    fflush(stdout);
	    i--;
	    j++;
	    if(i < 0){
		i = 4;
		cout << message << "[" << x[j % 3] << "]" << blank << "\r";
	    }
	    usleep(500000);
	}
    }
    
    //启用一个线程来做正在处理信息提示
    static void BeginShowMessage(string message)
    {
	pthread_create(&tid, NULL, Move, (void*)message.c_str());
    }

    static void EndShowMessage()
    {
	pthread_cancel(tid);
    }
};
pthread_t Util::tid;



//图灵机器人
class TuringRT{
private:
    //图灵机器人的请求地址
    string url = "http://openapi.tuling123.com/openapi/api/v2";
    //机器人信息
    string api_key = "67a04c4097594cb3bd6c3722b8569524";
    //用户id
    string user_id = "1";
    //这里请求发送的方式采用现成的百度语音识别Http Client
    aip::HttpClient client;

public:
    TuringRT()
    {}


    //创造一个图灵机器人接收的Json格式(序列化)
    string MakeJsonString(const string &message)
    {
	Json::Value root;
	Json::Value item;
	item["apiKey"] = api_key;
	item["userId"] = user_id;

	root["reqType"] = 0;
	root["userInfo"] = item;

	Json::Value item1;
	item1["text"] = message;
	Json::Value item2;
        item2["inputText"] = item1;
	root["perception"] = item2;
	Json::StreamWriterBuilder wb;
	ostringstream os;
	unique_ptr<Json::StreamWriter> jw(wb.newStreamWriter());
	jw->write(root ,&os);
	return os.str();
    }

    //通过百度语音识别现有的http client请求发起请求
    string RequestPost(string &body)
    {
	string response;
	int code = client.post(url, nullptr, body, nullptr, &response);
	if(code != CURLcode::CURLE_OK){
	    return "";
	}

	return response;
    }
    //将图灵机器人回复的响应信息拆解拿出(反序列化)
    string ParseJson(string &response)
    {
	JSONCPP_STRING errs;
	Json::Value root;
	Json::CharReaderBuilder rb;
	unique_ptr<Json::CharReader> const rp(rb.newCharReader());
	bool res = rp->parse(response.data(),\
	    response.data() + response.size(), &root, &errs);
	if(!res || !errs.empty()){
	    return "";
	}
	Json::Value item = root["results"][0];
	Json::Value item1 = item["values"];
	return item1["text"].asString();
    }
    //图灵机器人的对话功能
    void Talk(string message, string &result)
    {
	string body = MakeJsonString(message);
	//cout << body << endl;
	string response = RequestPost(body);
	//cout << response << endl;
	result = ParseJson(response);
	//cout << result << endl;
    }
    
    ~TuringRT()
    {}
};


//百度语音
class SpeechRec{
private:
    //注册百度账号后的账号信息
    static string app_id;
    static string api_key;
    static string secret_key;
    //SDK对应的语音识别客户端
    aip::Speech *client;

public:
    SpeechRec()
    {
	client = new aip::Speech(app_id, api_key, secret_key);
    }
    //语音识别
    string ASR(const string &voice_bin)
    {
	Util::BeginShowMessage("正在识别");
	//语音识别参数，详见百度文档
	map<string, string> options;
	//语言参数：中文
	options["dev_pid"] = "1536";


	//上传音频文件进行识别
	string file_content;
	aip::get_file_content(voice_bin.c_str(), &file_content);

	Json::Value result = client->recognize(file_content, "wav", \
	    16000, options);
	Util::EndShowMessage();

	//识别结果信息判断
	int code = result["err_no"].asInt();
	if(code != 0){
	    cerr << "code:" << code << "err_meg:" << \
		result["err_msg"].asString() << endl;
	    return "";
	}
	return result["result"][0].asString();
    }

    //语音合成
    void TTS(string &text, string voice_tts)
    {
	ofstream ofile;
	string ret;
	//设定合成语音信息
	map<string, string> options;
	options["spd"] = "5";
	options["per"] = "4";
	options["vol"] = "15";

	Util::BeginShowMessage("正在合成");
	ofile.open(voice_tts, ios::out | ios::binary);
	Json::Value result = client->text2audio(text, options, ret);
	//对合成信息进行判断
	if(ret.empty()){
	    cerr << result.toStyledString() << endl;
	}
	else{
	    ofile << ret << endl;
	}
	ofile.close();
	Util::EndShowMessage();
    }

    ~SpeechRec()
    {
	delete client;
	client = nullptr;
    }
};

string SpeechRec::app_id = "16727887";
string SpeechRec::api_key = "q1Nkfo7UyuCyEHsFTEBmKeWt";
string SpeechRec::secret_key = "b9fNGZ39bnh9OE0Ad4nOcm5Gt8OaAwLf";



//语音助手综合类
class Lewis{

private:
    TuringRT rt;
    SpeechRec sr;
    unordered_map <string, string> cmd_map;

public:
    Lewis()
    {}
   

    //录音功能
    bool RecordVoice()
    {
	Util::BeginShowMessage("正在录音");
	bool ret = true;
	string command = "arecord -t wav -c 1 -r 16000 -d 3 -f S16_LE ";
	command += VOICE_FILE;

	if(!Util::Exec(command, false)){
	    cerr << "Record error!" << endl;
	    ret = false;
	}
	Util::EndShowMessage();
	return ret;
    }



    //一些本地操作的配置
    void loadEtc(const string &etc)
    {
	//etc文件里存着文字和本地操作命令组成的键值对
	ifstream in(etc);
	if(!in.is_open()){
	    cerr << "open error!" << endl;
	    return;
	}
	
	string sep = ":";
	char buffer[BSIZE];
	while(in.getline(buffer, sizeof(buffer))){
	    string str = buffer;
	    size_t pos = str.find(sep);
	    if(string::npos == pos){
		cout << "command etc error" << endl;
		continue;
	    }
	    string k = str.substr(0, pos);
	    k+="。";
	    string v = str.substr(pos + sep.size());

	    //将这个检查无误的命令加入到命令的map中
	    cmd_map.insert(make_pair(k, v));
	}
	in.close();
    }

    //判断text是否为命令
    bool IsCommand(const string &text, string &out_command)
    {
	auto iter = cmd_map.find(text);
	if(iter != cmd_map.end()){
	    out_command = iter->second;
	    return true;
	}
	out_command = "";
	return false;
    }

    //本地录音
    void PlayVoice(string voice_file)
    {
	string cmd = "cvlc --play-and-exit ";
	cmd += voice_file;
	Util::Exec(cmd, false);
    }
    
    void Run()
    {
	string voice_bin = VOICE_FILE;
	//设立一个变量来选择什么时候退出聊天
	volatile bool is_quit = false;
	string command;
	while(!is_quit){
	    command = "";
	    if(RecordVoice()){
		string text = sr.ASR(voice_bin);
		cout << "我# " << text << endl;
		if(IsCommand(text, command)){
		    cout << "本地执行 " << command << endl;
		    Util::Exec(command, true);
		}
		else{
		    string message;
		    if(text == "退出。"){
			message = "欢迎使用，再见";
			is_quit = true;
		    }
		    else{
			rt.Talk(text, message);
			cout << "Lewis机器人# " << message << endl;
		    }
		    string voice_tts;
		    sr.TTS(message, VOICE_TTS_FILE);
		    PlayVoice(VOICE_TTS_FILE);		    
		}
	    }
	}
    }

    ~Lewis()
    {}

};

#endif


