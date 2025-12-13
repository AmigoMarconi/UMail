#pragma once
#include <string>
#include <vector>

struct Attachment {
    std::string filename;
    std::string mimeType;
    std::vector<unsigned char> data; 
};

struct SmtpSettings {
    std::string server = "localhost";
    unsigned short port = 25;

    bool useAuth = false;
    std::string user;
    std::string password;
};

struct MailDraft {
    std::string from;
    std::vector<std::string> to;
    std::string subject;
    std::string body;

    std::vector<Attachment> attachments;  
};

class MailService {
public:
    static bool send(const SmtpSettings& smtp, const MailDraft& draft, std::string& errorOut);

private:
    static std::string buildRawMessage(const MailDraft& draft);
    static bool hasNonAscii(const std::string& s);
};
