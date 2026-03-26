#include "core/update_checker.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace zibby::core {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0) {
        s.pop_back();
    }
    return s;
}

std::string stripLeadingV(std::string v) {
    v = trim(v);
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) {
        return v.substr(1);
    }
    return v;
}

std::vector<int> parseVersionParts(const std::string& version) {
    std::vector<int> parts;
    std::string token;
    for (char c : version) {
        if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
            token.push_back(c);
        } else if (c == '.') {
            if (!token.empty()) {
                parts.push_back(std::stoi(token));
                token.clear();
            } else {
                parts.push_back(0);
            }
        } else {
            // stop at first non version-ish character
            break;
        }
    }
    if (!token.empty()) {
        parts.push_back(std::stoi(token));
    }
    while (parts.size() < 4) {
        parts.push_back(0);
    }
    return parts;
}

int compareVersions(const std::string& a, const std::string& b) {
    const auto pa = parseVersionParts(a);
    const auto pb = parseVersionParts(b);
    const auto n = std::max(pa.size(), pb.size());
    for (std::size_t i = 0; i < n; ++i) {
        const int va = i < pa.size() ? pa[i] : 0;
        const int vb = i < pb.size() ? pb[i] : 0;
        if (va < vb) {
            return -1;
        }
        if (va > vb) {
            return 1;
        }
    }
    return 0;
}

bool readHttpResponse(const std::string& raw, int* statusCode, std::string* body) {
    const auto headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return false;
    }

    const std::string header = raw.substr(0, headerEnd);
    *body = raw.substr(headerEnd + 4);

    std::istringstream in(header);
    std::string http;
    int code = 0;
    in >> http >> code;
    if (code == 0) {
        return false;
    }

    *statusCode = code;
    return true;
}

std::string getEnvToken() {
    const char* t1 = std::getenv("ZIBBY_GITHUB_TOKEN");
    if (t1 != nullptr && *t1 != '\0') {
        return std::string(t1);
    }
    const char* t2 = std::getenv("GITHUB_TOKEN");
    if (t2 != nullptr && *t2 != '\0') {
        return std::string(t2);
    }
    return {};
}

std::string httpsGet(const std::string& host, const std::string& port, const std::string& target, const std::string& bearerToken, std::string* error) {
    try {
        boost::asio::io_context io;
        boost::asio::ssl::context sslCtx(boost::asio::ssl::context::tls_client);
        sslCtx.set_default_verify_paths();

        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> stream(io, sslCtx);

        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            throw std::runtime_error("SNI setup failed");
        }

        stream.set_verify_mode(boost::asio::ssl::verify_peer);

        boost::asio::ip::tcp::resolver resolver(io);
        const auto endpoints = resolver.resolve(host, port);
        boost::asio::connect(stream.next_layer(), endpoints);

        stream.handshake(boost::asio::ssl::stream_base::client);

        std::ostringstream request;
        request << "GET " << target << " HTTP/1.1\r\n";
        request << "Host: " << host << "\r\n";
        request << "User-Agent: zibby-service\r\n";
        request << "Accept: application/vnd.github+json\r\n";
        if (!bearerToken.empty()) {
            request << "Authorization: Bearer " << bearerToken << "\r\n";
        }
        request << "Connection: close\r\n\r\n";

        const std::string req = request.str();
        boost::asio::write(stream, boost::asio::buffer(req));

        std::string response;
        boost::system::error_code ec;
        for (;;) {
            char buf[4096];
            const std::size_t bytes = stream.read_some(boost::asio::buffer(buf), ec);
            if (bytes > 0) {
                response.append(buf, buf + bytes);
            }
            if (ec == boost::asio::error::eof) {
                break;
            }
            if (ec) {
                throw boost::system::system_error(ec);
            }
        }

        return response;
    } catch (const std::exception& ex) {
        if (error) {
            *error = ex.what();
        }
        return {};
    }
}

} // namespace

bool UpdateChecker::parseRepoUrl(const std::string& repoUrl, std::string* owner, std::string* repo) {
    if (owner == nullptr || repo == nullptr) {
        return false;
    }

    std::string url = trim(repoUrl);
    url = toLower(url);

    const std::string prefix = "https://github.com/";
    std::size_t pos = 0;
    if (url.rfind(prefix, 0) == 0) {
        pos = prefix.size();
    } else {
        const std::string prefix2 = "http://github.com/";
        if (url.rfind(prefix2, 0) == 0) {
            pos = prefix2.size();
        } else {
            return false;
        }
    }

    std::string rest = url.substr(pos);
    while (!rest.empty() && rest.back() == '/') {
        rest.pop_back();
    }

    const auto slash = rest.find('/');
    if (slash == std::string::npos || slash == 0 || slash == rest.size() - 1) {
        return false;
    }

    *owner = rest.substr(0, slash);
    *repo = rest.substr(slash + 1);

    // strip possible .git
    const std::string gitSuffix = ".git";
    if (repo->size() > gitSuffix.size() && repo->rfind(gitSuffix) == repo->size() - gitSuffix.size()) {
        *repo = repo->substr(0, repo->size() - gitSuffix.size());
    }

    return !owner->empty() && !repo->empty();
}

UpdateCheckResult UpdateChecker::checkLatestRelease(const std::string& repoUrl, const std::string& currentVersion) {
    UpdateCheckResult result;

    std::string owner;
    std::string repo;
    if (!parseRepoUrl(repoUrl, &owner, &repo)) {
        result.error = "invalid_repo_url";
        return result;
    }

    const std::string target = "/repos/" + owner + "/" + repo + "/releases/latest";

    std::string error;
    const auto raw = httpsGet("api.github.com", "443", target, getEnvToken(), &error);
    if (raw.empty()) {
        result.error = std::string("network_error: ") + error;
        return result;
    }

    int status = 0;
    std::string body;
    if (!readHttpResponse(raw, &status, &body)) {
        result.error = "invalid_http_response";
        return result;
    }

    if (status < 200 || status >= 300) {
        // Try to extract GitHub error message for better diagnostics.
        try {
            boost::property_tree::ptree errTree;
            std::istringstream in(body);
            boost::property_tree::read_json(in, errTree);
            const auto message = errTree.get<std::string>("message", "");
            if (!message.empty()) {
                result.error = "http_status_" + std::to_string(status) + ": " + message;
                return result;
            }
        } catch (...) {
        }
        result.error = "http_status_" + std::to_string(status);
        return result;
    }

    try {
        boost::property_tree::ptree tree;
        std::istringstream in(body);
        boost::property_tree::read_json(in, tree);

        std::string tag = tree.get<std::string>("tag_name", "");
        if (tag.empty()) {
            tag = tree.get<std::string>("name", "");
        }
        tag = stripLeadingV(tag);

        result.ok = true;
        result.latestVersion = tag;

        const std::string current = stripLeadingV(currentVersion);
        if (!tag.empty() && !current.empty()) {
            result.updateAvailable = compareVersions(current, tag) < 0;
        }
        return result;
    } catch (const std::exception& ex) {
        result.error = std::string("json_parse_error: ") + ex.what();
        return result;
    }
}

} // namespace zibby::core
