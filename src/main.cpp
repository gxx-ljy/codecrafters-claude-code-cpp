#include <cstdlib>
#include <iostream>
#include <string>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <streambuf>

using json = nlohmann::json;

std::string read_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
}

int main(int argc, char* argv[]) {
    if (argc < 3 || std::string(argv[1]) != "-p") {
        std::cerr << "Expected first argument to be '-p'" << std::endl;
        return 1;
    }

    std::string prompt = argv[2];

    if (prompt.empty()) {
        std::cerr << "Prompt must not be empty" << std::endl;
        return 1;
    }

    const char* api_key_env = std::getenv("OPENROUTER_API_KEY");
    const char* base_url_env = std::getenv("OPENROUTER_BASE_URL");

    std::string api_key = api_key_env ? api_key_env : "";
    std::string base_url = base_url_env ? base_url_env : "https://openrouter.ai/api/v1";

    if (api_key.empty()) {
        std::cerr << "OPENROUTER_API_KEY is not set" << std::endl;
        return 1;
    }

    json tools = json::array({
        {
            {"type", "function"},
            {"function", {
                {"name", "Read"},
                {"description", "Read and return the contents of a file"},
                {"parameters", {
                    {"type", "object"},
                    {"properties", {
                        {"file_path", {
                            {"type", "string"},
                            {"description", "The path to the file to read"}
                        }}
                    }},
                    {"required", json::array({"file_path"})}
                }}
            }}
        },
    });

    json messages = json::array({
        {{"role", "user"}, {"content", prompt}},
    });

    while (true) {
        json request_body = {
            {"model", "anthropic/claude-haiku-4.5"},
            {"messages", messages},
            {"tools", tools},
        };

        cpr::Response response = cpr::Post(
            cpr::Url{base_url + "/chat/completions"},
            cpr::Header{
                {"Authorization", "Bearer " + api_key},
                {"Content-Type", "application/json"}
            },
            cpr::Body{request_body.dump()}
        );

        if (response.status_code != 200) {
            std::cerr << "HTTP error: " << response.status_code << std::endl;
            return 1;
        }

        json result = json::parse(response.text);

        if (!result.contains("choices") || result["choices"].empty()) {
            std::cerr << "No choices in response" << std::endl;
            return 1;
        }

        // You can use print statements as follows for debugging, they'll be visible when running tests.
        std::cerr << "Logs from your program will appear here!" << std::endl;

        json assistant_message = result["choices"][0]["message"];
        messages.push_back(assistant_message);

        json tool_calls = result["choices"][0]["message"]["tool_calls"];
        if (!result["choices"][0]["message"].contains("tool_calls") ||result["choices"][0]["message"]["tool_calls"].empty()) {
            std::cout << result["choices"][0]["message"]["content"].get<std::string>();
            break;
        }

        for (const auto& tc : tool_calls) {
            json args = json::parse(tc["function"]["arguments"].get<std::string>());
            if (tc["function"]["name"].get<std::string>() == "Read") {
                std::string file_path = args["file_path"].get<std::string>();
                std::string content = read_file(file_path);
                // std::cout << content;
                messages.push_back({
                    {"role", "tool"},
                    {"tool_call_id", tc["id"]},
                    {"content", content}
                });
            } else {
                std::cout << result["choices"][0]["message"]["content"].get<std::string>();
                break;
            }
        }
    }
    return 0;
}
