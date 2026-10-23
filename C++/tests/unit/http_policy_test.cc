#include <gtest/gtest.h>
#include "server/http_policy.h"

namespace {

using oj::http_policy::applyCachePolicy;
using oj::http_policy::cookieValue;
using oj::http_policy::sessionToken;

TEST(HttpCookiePolicy, FindsOnlyTheExactCaseSensitiveCookieName) {
    httplib::Request request;
    request.set_header("Cookie", "not_session_id=wrong; session_id_extra=wrong; Session_id=wrong; session_id=valid");
    EXPECT_EQ(sessionToken(request), "valid");
    EXPECT_FALSE(cookieValue("not_session_id=wrong; Session_id=wrong", "session_id").has_value());
}

TEST(HttpCookiePolicy, HandlesWhitespaceMalformedPairsAndEqualsInValues) {
    EXPECT_EQ(cookieValue(" ; broken;\t session_id = token=with=equals \t; other=value", "session_id"),
              std::optional<std::string>("token=with=equals"));
    EXPECT_FALSE(cookieValue("; broken; missing=;", "session_id").has_value());
}

TEST(HttpCookiePolicy, HandlesMissingAndEmptyCookiesWithoutChoosingAnotherToken) {
    httplib::Request request;
    EXPECT_TRUE(sessionToken(request).empty());
    request.set_header("Cookie", "session_id=; session_id=other-token");
    EXPECT_TRUE(sessionToken(request).empty());
    EXPECT_EQ(cookieValue("session_id=", "session_id"), std::optional<std::string>(""));
}

TEST(HttpCookiePolicy, ReadsMultipleCaseInsensitiveCookieHeaderFields) {
    httplib::Request request;
    request.set_header("cookie", "theme=light");
    request.set_header("Cookie", "session_id=valid; other=1");
    EXPECT_EQ(sessionToken(request), "valid");
}

TEST(HttpCachePolicy, CoversHtmlApiCssAndJavaScriptPaths) {
    for (const auto* path : {"/", "/index.html", "/login.html", "/REGISTER.HTML", "/api", "/api/login",
                             "/api/problems/12", "/css/style.css", "/js/auth.js", "/login.html?v=release"}) {
        SCOPED_TRACE(path);
        httplib::Request request;
        request.path = path;
        httplib::Response response;
        applyCachePolicy(request, response);
        EXPECT_EQ(response.get_header_value("Cache-Control"), "no-store, max-age=0");
        EXPECT_EQ(response.get_header_value("Pragma"), "no-cache");
    }
}

TEST(HttpCachePolicy, ReplacesConflictingHeadersAndIsIdempotent) {
    httplib::Request request;
    request.path = "/api/problems";
    httplib::Response response;
    response.set_header("cache-control", "public, max-age=3600");
    response.set_header("Cache-Control", "immutable");
    response.set_header("pragma", "cache");
    applyCachePolicy(request, response);
    applyCachePolicy(request, response);
    EXPECT_EQ(response.headers.count("Cache-Control"), 1u);
    EXPECT_EQ(response.headers.count("Pragma"), 1u);
    EXPECT_EQ(response.get_header_value("Cache-Control"), "no-store, max-age=0");
}

TEST(HttpCachePolicy, RemovesStaticValidatorsForGetHeadAndNotModifiedResponses) {
    for (const auto* method : {"GET", "HEAD"}) {
        for (const int status : {200, 304, 404}) {
            for (const auto* path : {"/", "/problem_list.html", "/css/style.css", "/js/api.js"}) {
                SCOPED_TRACE(std::string(method) + " " + path + " " + std::to_string(status));
                httplib::Request request;
                request.method = method;
                request.path = path;
                httplib::Response response;
                response.status = status;
                response.set_header("ETag", "W/\"old\"");
                response.set_header("Last-Modified", "Thu, 22 Oct 2026 00:00:00 GMT");
                applyCachePolicy(request, response);
                EXPECT_FALSE(response.has_header("ETag"));
                EXPECT_FALSE(response.has_header("Last-Modified"));
                EXPECT_EQ(response.get_header_value("Cache-Control"), "no-store, max-age=0");
                EXPECT_EQ(response.status, status);
            }
        }
    }
}

TEST(HttpCachePolicy, PreservesResponseBodyStatusContentTypeAndCookies) {
    httplib::Request request;
    request.path = "/api/login";
    httplib::Response response;
    response.status = 401;
    response.set_content("{\"error\":\"unauthorized\"}", "application/json");
    response.set_header("Set-Cookie", "other=value; Path=/");
    applyCachePolicy(request, response);
    EXPECT_EQ(response.status, 401);
    EXPECT_EQ(response.body, "{\"error\":\"unauthorized\"}");
    EXPECT_EQ(response.get_header_value("Content-Type"), "application/json");
    EXPECT_EQ(response.get_header_value("Set-Cookie"), "other=value; Path=/");
}

TEST(HttpCachePolicy, LeavesUnrelatedResourcesAndSimilarApiPrefixesAlone) {
    for (const auto* path : {"/images/logo.png", "/robots.txt", "/js/auth.js.map", "/apix/problems", "/index.html.backup"}) {
        SCOPED_TRACE(path);
        httplib::Request request;
        request.path = path;
        httplib::Response response;
        response.set_header("Cache-Control", "public, max-age=60");
        response.set_header("ETag", "W/\"asset\"");
        applyCachePolicy(request, response);
        EXPECT_EQ(response.get_header_value("Cache-Control"), "public, max-age=60");
        EXPECT_EQ(response.get_header_value("ETag"), "W/\"asset\"");
        EXPECT_FALSE(response.has_header("Pragma"));
    }
}

TEST(HttpSessionCookiePolicy, LoginKeepsBrowserSessionSemanticsAndSharedScope) {
    httplib::Response response;
    oj::http_policy::setSessionCookie(response, "server-token");
    const auto cookie = response.get_header_value("Set-Cookie");
    EXPECT_EQ(cookie, "session_id=server-token; Path=/; HttpOnly; SameSite=Lax");
    EXPECT_EQ(cookie.find("Max-Age"), std::string::npos);
    EXPECT_EQ(cookie.find("Expires"), std::string::npos);
}

TEST(HttpSessionCookiePolicy, LogoutExpiresTheCookieWithoutChangingErrorResponses) {
    for (const int status : {200, 400, 500}) {
        httplib::Response response;
        response.status = status;
        response.set_content("{\"message\":\"unchanged\"}", "application/json");
        oj::http_policy::expireSessionCookie(response);
        EXPECT_EQ(response.get_header_value("Set-Cookie"),
                  "session_id=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax");
        EXPECT_EQ(response.status, status);
        EXPECT_EQ(response.body, "{\"message\":\"unchanged\"}");
    }
}

} // namespace
