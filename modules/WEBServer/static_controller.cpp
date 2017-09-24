#include "static_controller.hpp"

#include <boost/algorithm/string.hpp>

#include <fstream>

#define BUF_SIZE 4096



bool nonAsciiChar(const char c) {
	return !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_');
}
void stripNonAscii(string &str) {
	str.erase(std::remove_if(str.begin(), str.end(), nonAsciiChar), str.end());
}


bool nonPathChar(const char c) {
	return !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '/' || c == '-');
}
std::string stripPath(string str) {
	str.erase(std::remove_if(str.begin(), str.end(), nonPathChar), str.end());
	return str;
}
StaticController::StaticController(boost::shared_ptr<session_manager_interface> session, std::string path) 
	: session(session)
	, base(path) 
{}


Mongoose::Response* StaticController::handleRequest(Mongoose::Request &request) {
  bool is_js = boost::algorithm::ends_with(request.getUrl(), ".js");
  bool is_css = boost::algorithm::ends_with(request.getUrl(), ".css");
  bool is_html = boost::algorithm::ends_with(request.getUrl(), ".html");
  bool is_gif = boost::algorithm::ends_with(request.getUrl(), ".gif");
  bool is_png = boost::algorithm::ends_with(request.getUrl(), ".png");
  bool is_jpg = boost::algorithm::ends_with(request.getUrl(), ".jpg");
  bool is_font = boost::algorithm::ends_with(request.getUrl(), ".ttf")
    || boost::algorithm::ends_with(request.getUrl(), ".svg")
    || boost::algorithm::ends_with(request.getUrl(), ".woff");
  Mongoose::StreamResponse *sr = new Mongoose::StreamResponse();
  if (!is_js && !is_html && !is_css && !is_font && !is_jpg && !is_gif && !is_png) {
    sr->setCode(HTTP_NOT_FOUND);
    sr->append("Not found: " + request.getUrl());
    return sr;
  }
  std::string path = stripPath(request.getUrl());
  if (path.find("..") != std::string::npos) {
	  sr->setCode(HTTP_SERVER_ERROR);
	  sr->append("Invalid path: " + path);
	  return sr;
  }

  boost::filesystem::path file = base / path;
  if (!boost::filesystem::is_regular_file(file)) {
    sr->setCode(HTTP_NOT_FOUND);
    sr->append("Not found: " + path);
    return sr;
  }

  if (is_js)
    sr->setHeader("Content-Type", "application/javascript");
  else if (is_css)
    sr->setHeader("Content-Type", "text/css");
  else if (is_font)
    sr->setHeader("Content-Type", "text/html");
  else if (is_gif)
    sr->setHeader("Content-Type", "image/gif");
  else if (is_jpg)
    sr->setHeader("Content-Type", "image/jpeg");
  else if (is_png)
    sr->setHeader("Content-Type", "image/png");
  else {
    sr->setHeader("Content-Type", "text/html");
  }
  if (is_css || is_font || is_gif || is_png || is_jpg || is_js) {
    sr->setHeader("Cache-Control", "max-age=3600"); //1 hour (60*60)
  }
  std::ifstream in(file.string().c_str(), ios_base::in | ios_base::binary);
  char buf[BUF_SIZE];

  std::string token = request.get("__TOKEN");
  if (!session->validate_token(token))
    token = "";
  if (is_html) {
    std::string line;
    while (std::getline(in, line)) {
      if (line.empty())
        continue;
      std::string::size_type pos = line.find("<%=");
      if (pos != std::string::npos) {
        std::string::size_type end = line.find("%>", pos);
        if (end != std::string::npos) {
          pos += 3;
          std::string key = line.substr(pos, end - pos);
          if (boost::starts_with(key, "INCLUDE:")) {
            std::string fname = key.substr(8);
            stripNonAscii(fname);
            fname += ".html";
            boost::filesystem::path file2 = base / "include" / fname;
            std::ifstream in2(file2.string().c_str(), ios_base::in | ios_base::binary);
            do {
              in2.read(&buf[0], BUF_SIZE);
              sr->write(&buf[0], in2.gcount());
            } while (in2.gcount() > 0);
            in2.close();
            line = line.substr(0, pos - 3) + line.substr(end + 2);
            boost::replace_all(line, "<%=TOKEN%>", token);
          } else {
            boost::replace_all(line, "<%=TOKEN%>", token);
            if (!token.empty())
              boost::replace_all(line, "<%=TOKEN_TAG%>", "?__TOKEN=" + token);
            else
              boost::replace_all(line, "<%=TOKEN_TAG%>", "");
          }
        }
      }
      sr->write(line.c_str(), line.size());
    }
  } else {
    do {
      in.read(&buf[0], BUF_SIZE);
      sr->write(&buf[0], in.gcount());
    } while (in.gcount() > 0);
  }
  in.close();
  return sr;
}
bool StaticController::handles(string method, string url) {
  return boost::algorithm::ends_with(url, ".js")
    || boost::algorithm::ends_with(url, ".css")
    || boost::algorithm::ends_with(url, ".html")
    || boost::algorithm::ends_with(url, ".ttf")
    || boost::algorithm::ends_with(url, ".svg")
    || boost::algorithm::ends_with(url, ".woff")
    || boost::algorithm::ends_with(url, ".gif")
    || boost::algorithm::ends_with(url, ".png")
    || boost::algorithm::ends_with(url, ".jpg");
  ;
}
