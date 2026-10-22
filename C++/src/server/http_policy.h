#pragma once

#include <httplib.h>
#include <optional>
#include <string>
#include <string_view>

namespace oj::http_policy {

inline std::string_view trimCookieWhitespace(std::string_view value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string_view::npos) return {};
    const auto last = value.find_last_not_of(" \t");
    return value.substr(first, last - first + 1);
}

// Cookie names are case-sensitive. Split pairs before matching the full name;
// substring searches would also accept names such as "not_session_id".
inline std::optional<std::string> cookieValue(std::string_view header,
                                            std::string_view name) {
    while (!header.empty()) {
        const auto separator = header.find(';');
        const auto pair = trimCookieWhitespace(header.substr(0, separator));
        const auto equals = pair.find('=');
        if (equals != std::string_view::npos &&
            trimCookieWhitespace(pair.substr(0, equals)) == name) {
            return std::string(trimCookieWhitespace(pair.substr(equals + 1)));
        }
        if (separator == std::string_view::npos) break;
        header.remove_prefix(separator + 1);
    }
    return std::nullopt;
}

inline std::string sessionToken(const httplib::Request& request) {
    const auto headers = request.headers.equal_range("Cookie");
    for (auto it = headers.first; it != headers.second; ++it) {
        const auto value = cookieValue(it->second, "session_id");
        if (value.has_value()) return *value;
    }
    return {};
}

inline std::string_view pathWithoutQuery(std::string_view path) {
    return path.substr(0, path.find_first_of("?#"));
}

inline bool hasExtension(std::string_view path, std::string_view extension) {
    if (path.size() < extension.size()) return false;
    const auto suffix = path.substr(path.size() - extension.size());
    for (std::size_t i = 0; i < suffix.size(); ++i) {
        const char character = suffix[i];
        const char lower = character >= 'A' && character <= 'Z'
            ? static_cast<char>(character - 'A' + 'a') : character;
        if (lower != extension[i]) return false;
    }
    return true;
}

// Called after routing so GET, HEAD, errors and conditional 304 responses all
// receive the same policy. Static validators are not useful with no-store.
inline void applyCachePolicy(const httplib::Request& request,
                             httplib::Response& response) {
    const auto path = pathWithoutQuery(request.path);
    const bool api = path == "/api" || path.substr(0, 5) == "/api/";
    const bool app_file = !api && (path == "/" || hasExtension(path, ".html") ||
                                  hasExtension(path, ".css") || hasExtension(path, ".js"));
    if (!api && !app_file) return;

    // cpp-httplib set_header appends, rather than replacing existing values.
    response.headers.erase("Cache-Control");
    response.headers.erase("Pragma");
    response.set_header("Cache-Control", "no-store, max-age=0");
    response.set_header("Pragma", "no-cache");
    if (app_file) {
        response.headers.erase("ETag");
        response.headers.erase("Last-Modified");
    }
}

inline void setSessionCookie(httplib::Response& response, const std::string& token) {
    response.set_header("Set-Cookie", "session_id=" + token +
                        "; Path=/; HttpOnly; SameSite=Lax");
}

inline void expireSessionCookie(httplib::Response& response) {
    response.set_header("Set-Cookie",
                        "session_id=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax");
}

} // namespace oj::http_policy
