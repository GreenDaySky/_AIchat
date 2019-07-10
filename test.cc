#include <iostream>
#include <sstream>
#include <memory>
#include <string>
#include <json/json.h>

int main()
{
    Json::Value root;
    Json::StreamWriterBuilder wb;
    std::ostringstream os;

    root["Name"] = "张三";
    root["age"] = 26;
    root["Lang"] = "C++";

    std::unique_ptr<Json::StreamWriter> jsonWriter(wb.newStreamWriter());
    jsonWriter->write(root, &os);
    std::string s = os.str();

    std::cout << s << std::endl;

    return 0;
}
