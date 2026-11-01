#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <cstdlib>
#include <cctype>
#include <filesystem>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
  using socket_t = SOCKET;
  const socket_t INVALID_SOCK = INVALID_SOCKET;
  void close_socket(socket_t s) { closesocket(s); }
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <signal.h>
  using socket_t = int;
  const socket_t INVALID_SOCK = -1;
  void close_socket(socket_t s) { close(s); }
#endif

namespace fs = std::filesystem;
static const int PORT = 8080;
static const std::string DATA_DIR = "data";
static const std::string TX_FILE = DATA_DIR + "/transactions.csv";
static const std::string BUDGET_FILE = DATA_DIR + "/budgets.csv";
static const std::string WEB_DIR = "frontend";

struct Transaction {
    std::string id, type, category, date, note;
    double amount = 0;
};

struct Budget {
    std::string category, month;
    double amount = 0;
};

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \r\n\t");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \r\n\t");
    return s.substr(a, b-a+1);
}

std::string csvEscape(const std::string& s) {
    std::string out = "\"";
    for (char c : s) { if (c == '"') out += "\"\""; else out += c; }
    return out + "\"";
}
std::vector<std::string> parseCsvLine(const std::string& line) {
    std::vector<std::string> out;
    std::string cur; bool quoted = false;
    for (size_t i=0; i<line.size(); ++i) {
        char c=line[i];
        if (c=='"') {
            if (quoted && i+1<line.size() && line[i+1]=='"') { cur+='"'; ++i; }
            else quoted=!quoted;
        } else if (c==',' && !quoted) { out.push_back(cur); cur.clear(); }
        else cur+=c;
    }
    out.push_back(cur);
    return out;
}
std::string jsonEscape(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        switch(c) {
            case '"': o+="\\\""; break; case '\\': o+="\\\\"; break;
            case '\n': o+="\\n"; break; case '\r': o+="\\r"; break;
            case '\t': o+="\\t"; break;
            default: if(c<32) { char b[8]; std::snprintf(b,sizeof(b),"\\u%04x",c); o+=b; } else o+=char(c);
        }
    }
    return o;
}
std::string urlDecode(const std::string& s) {
    std::string out;
    for (size_t i=0;i<s.size();++i) {
        if(s[i]=='+') out+=' ';
        else if(s[i]=='%' && i+2<s.size()) {
            try { out+=char(std::stoi(s.substr(i+1,2),nullptr,16)); i+=2; }
            catch(...) { out+=s[i]; }
        } else out+=s[i];
    }
    return out;
}
std::map<std::string,std::string> parseParams(const std::string& body) {
    std::map<std::string,std::string> p;
    std::stringstream ss(body); std::string item;
    while(std::getline(ss,item,'&')) {
        auto pos=item.find('=');
        if(pos!=std::string::npos) p[urlDecode(item.substr(0,pos))]=urlDecode(item.substr(pos+1));
    }
    return p;
}
std::string today() {
    std::time_t t=std::time(nullptr); std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm,&t);
#else
    localtime_r(&t,&tm);
#endif
    char b[16]; std::strftime(b,sizeof(b),"%Y-%m-%d",&tm); return b;
}
std::string currentMonth() { return today().substr(0,7); }

void ensureFiles() {
    fs::create_directories(DATA_DIR);
    if(!fs::exists(TX_FILE)) { std::ofstream f(TX_FILE); f<<"id,type,category,amount,date,note\n"; }
    if(!fs::exists(BUDGET_FILE)) { std::ofstream f(BUDGET_FILE); f<<"category,month,amount\n"; }
}
std::vector<Transaction> loadTransactions() {
    std::vector<Transaction> v; std::ifstream f(TX_FILE); std::string line;
    std::getline(f,line);
    while(std::getline(f,line)) {
        auto x=parseCsvLine(line); if(x.size()<6) continue;
        try { v.push_back({x[0],x[1],x[2],x[4],x[5],std::stod(x[3])}); } catch(...) {}
    } return v;
}
void saveTransactions(const std::vector<Transaction>& v) {
    std::ofstream f(TX_FILE,std::ios::trunc); f<<"id,type,category,amount,date,note\n";
    for(const auto& t:v) f<<csvEscape(t.id)<<','<<csvEscape(t.type)<<','<<csvEscape(t.category)<<','<<std::fixed<<std::setprecision(2)<<t.amount<<','<<csvEscape(t.date)<<','<<csvEscape(t.note)<<'\n';
}
std::vector<Budget> loadBudgets() {
    std::vector<Budget> v; std::ifstream f(BUDGET_FILE); std::string line; std::getline(f,line);
    while(std::getline(f,line)) { auto x=parseCsvLine(line); if(x.size()<3) continue; try { v.push_back({x[0],x[1],std::stod(x[2])}); } catch(...) {} }
    return v;
}
void saveBudgets(const std::vector<Budget>& v) {
    std::ofstream f(BUDGET_FILE,std::ios::trunc); f<<"category,month,amount\n";
    for(const auto& b:v) f<<csvEscape(b.category)<<','<<csvEscape(b.month)<<','<<std::fixed<<std::setprecision(2)<<b.amount<<'\n';
}
std::string txJson(const Transaction& t) {
    std::ostringstream o; o<<"{\"id\":\""<<jsonEscape(t.id)<<"\",\"type\":\""<<jsonEscape(t.type)<<"\",\"category\":\""<<jsonEscape(t.category)<<"\",\"amount\":"<<std::fixed<<std::setprecision(2)<<t.amount<<",\"date\":\""<<jsonEscape(t.date)<<"\",\"note\":\""<<jsonEscape(t.note)<<"\"}";
    return o.str();
}
std::string budgetsJson() {
    auto bs=loadBudgets(); std::ostringstream o; o<<"[";
    for(size_t i=0;i<bs.size();++i) { if(i)o<<","; o<<"{\"category\":\""<<jsonEscape(bs[i].category)<<"\",\"month\":\""<<jsonEscape(bs[i].month)<<"\",\"amount\":"<<bs[i].amount<<"}"; }
    return o.str()+"]";
}
std::string transactionsJson() {
    auto ts=loadTransactions(); std::sort(ts.begin(),ts.end(),[](auto&a,auto&b){return a.date>b.date;});
    std::ostringstream o; o<<"["; for(size_t i=0;i<ts.size();++i){if(i)o<<",";o<<txJson(ts[i]);} return o.str()+"]";
}
std::string summaryJson() {
    auto ts=loadTransactions(); std::string m=currentMonth();
    double income=0,expense=0,monthIncome=0,monthExpense=0;
    for(auto&t:ts) {
        if(t.type=="income") income+=t.amount; else expense+=t.amount;
        if(t.date.rfind(m,0)==0) { if(t.type=="income")monthIncome+=t.amount;else monthExpense+=t.amount; }
    }
    std::ostringstream o; o<<std::fixed<<std::setprecision(2)
      <<"{\"totalIncome\":"<<income<<",\"totalExpense\":"<<expense<<",\"balance\":"<<(income-expense)
      <<",\"monthIncome\":"<<monthIncome<<",\"monthExpense\":"<<monthExpense<<",\"monthBalance\":"<<(monthIncome-monthExpense)
      <<",\"month\":\""<<m<<"\",\"count\":"<<ts.size()<<"}";
    return o.str();
}
std::string makeId() {
    static unsigned long n=0; return std::to_string(std::time(nullptr))+"-"+std::to_string(++n);
}
bool validDate(const std::string& d) {
    if(d.size()!=10 || d[4]!='-' || d[7]!='-') return false;
    for(size_t i=0;i<d.size();++i) if(i!=4&&i!=7&&!std::isdigit((unsigned char)d[i])) return false;
    return true;
}
std::string contentType(const std::string& path) {
    auto p=path.find_last_of('.');
    std::string e=p==std::string::npos?"":path.substr(p);
    if(e==".html")return "text/html; charset=utf-8";
    if(e==".css")return "text/css";
    if(e==".js")return "application/javascript";
    if(e==".svg")return "image/svg+xml";
    return "text/plain";
}
void sendResponse(socket_t client,int code,const std::string& type,const std::string& body) {
    std::string status=code==200?"OK":code==201?"Created":code==400?"Bad Request":code==404?"Not Found":code==405?"Method Not Allowed":"Internal Server Error";
    std::ostringstream h; h<<"HTTP/1.1 "<<code<<" "<<status<<"\r\nContent-Type: "<<type<<"\r\nContent-Length: "<<body.size()<<"\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n";
    std::string response=h.str()+body; size_t sent=0;
    while(sent<response.size()) { int n=send(client,response.c_str()+sent,(int)(response.size()-sent),0); if(n<=0)break; sent+=n; }
}
void handle(socket_t client) {
    std::string raw; char buf[8192]; int n;
    while((n=recv(client,buf,sizeof(buf),0))>0) { raw.append(buf,n); if(raw.find("\r\n\r\n")!=std::string::npos) {
        size_t end=raw.find("\r\n\r\n"); size_t cl=0; auto cp=raw.find("Content-Length:");
        if(cp!=std::string::npos) { auto e=raw.find("\r\n",cp); try{cl=std::stoul(raw.substr(cp+15,e-cp-15));}catch(...){} }
        if(raw.size()>=end+4+cl)break;
    } if(raw.size()>2*1024*1024)break; }
    auto sep=raw.find("\r\n\r\n"); if(sep==std::string::npos){sendResponse(client,400,"text/plain","Bad request");return;}
    std::string head=raw.substr(0,sep), body=raw.substr(sep+4);
    std::istringstream hs(head); std::string method,target,version; hs>>method>>target>>version;
    auto q=target.find('?'); std::string path=target.substr(0,q), query=q==std::string::npos?"":target.substr(q+1);
    auto params=parseParams(body); auto qp=parseParams(query);
    if(method=="OPTIONS"){sendResponse(client,200,"text/plain","");return;}
    if(path=="/api/transactions" && method=="GET"){sendResponse(client,200,"application/json",transactionsJson());return;}
    if(path=="/api/summary" && method=="GET"){sendResponse(client,200,"application/json",summaryJson());return;}
    if(path=="/api/budgets" && method=="GET"){sendResponse(client,200,"application/json",budgetsJson());return;}
    if(path=="/api/transactions" && method=="POST") {
        std::string type=params["type"],cat=trim(params["category"]),date=params["date"],note=params["note"];
        double amount=0; try{amount=std::stod(params["amount"]);}catch(...){}
        if((type!="income"&&type!="expense")||cat.empty()||amount<=0||!validDate(date)){sendResponse(client,400,"application/json","{\"error\":\"Enter a valid type, category, positive amount and date.\"}");return;}
        auto ts=loadTransactions(); Transaction t{makeId(),type,cat,date,note,amount}; ts.push_back(t); saveTransactions(ts);
        sendResponse(client,201,"application/json",txJson(t));return;
    }
    if(path=="/api/transactions" && method=="DELETE") {
        std::string id=qp["id"]; auto ts=loadTransactions(); auto old=ts.size();
        ts.erase(std::remove_if(ts.begin(),ts.end(),[&](auto&t){return t.id==id;}),ts.end());
        if(old==ts.size()){sendResponse(client,404,"application/json","{\"error\":\"Transaction not found.\"}");return;}
        saveTransactions(ts);sendResponse(client,200,"application/json","{\"success\":true}");return;
    }
    if(path=="/api/budgets" && method=="POST") {
        std::string cat=trim(params["category"]),month=params["month"]; double amount=0;try{amount=std::stod(params["amount"]);}catch(...){}
        if(cat.empty()||month.size()!=7||amount<=0){sendResponse(client,400,"application/json","{\"error\":\"Enter a category, valid month and positive budget.\"}");return;}
        auto bs=loadBudgets(); bool found=false;
        for(auto&b:bs)if(b.category==cat&&b.month==month){b.amount=amount;found=true;}
        if(!found)bs.push_back({cat,month,amount}); saveBudgets(bs);
        sendResponse(client,200,"application/json","{\"success\":true}");return;
    }
    if(path.rfind("/api/",0)==0){sendResponse(client,404,"application/json","{\"error\":\"API route not found.\"}");return;}
    if(method!="GET"){sendResponse(client,405,"text/plain","Method not allowed");return;}
    if(path=="/")path="/index.html";
    std::string rel=path.substr(1);
    if(rel.find("..")!=std::string::npos){sendResponse(client,400,"text/plain","Invalid path");return;}
    fs::path file=fs::path(WEB_DIR)/rel;
    if(!fs::exists(file)||!fs::is_regular_file(file)){sendResponse(client,404,"text/plain","File not found");return;}
    std::ifstream f(file,std::ios::binary);std::ostringstream content;content<<f.rdbuf();
    sendResponse(client,200,contentType(file.string()),content.str());
}
int main() {
    ensureFiles();
#ifdef _WIN32
    WSADATA wsa; if(WSAStartup(MAKEWORD(2,2),&wsa)!=0){std::cerr<<"Winsock startup failed\n";return 1;}
#else
    signal(SIGPIPE,SIG_IGN);
#endif
    socket_t server=socket(AF_INET,SOCK_STREAM,0);
    if(server==INVALID_SOCK){std::cerr<<"Could not create server socket\n";return 1;}
#ifndef _WIN32
    int opt=1;setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
#endif
    sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=INADDR_ANY;addr.sin_port=htons(PORT);
    if(bind(server,(sockaddr*)&addr,sizeof(addr))<0||listen(server,10)<0){std::cerr<<"Could not bind port "<<PORT<<". Is it already in use?\n";close_socket(server);return 1;}
    std::cout<<"========================================\n";
    std::cout<<"       SMART EXPENSE TRACKER (C++)\n";
    std::cout<<"========================================\n";
    std::cout<<"Server running at http://localhost:"<<PORT<<"\n";
    std::cout<<"Keep this terminal open. Press Ctrl+C to stop.\n";
    while(true){socket_t client=accept(server,nullptr,nullptr);if(client==INVALID_SOCK)continue;handle(client);close_socket(client);}
    close_socket(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
