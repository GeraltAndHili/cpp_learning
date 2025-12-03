/* 该文件其实就是把redis_reg_log_demo.cpp的主体拿过来作为头文件了，该cpp是为了测试redis登录和注册功能

本.h文件创建是为了把功能打包，便于其他模块调用
*/

#include<iostream>
#include<string>
#include<vector>
#include<sstream>
#include<iomanip>
#include<openssl/md5.h>
#include<hiredis/hiredis.h>
#include "ConnectionPool.h"
#include"connectionpool_declare.h"

using namespace std;

//辅助用的md5
string md5_hex(const string& input){
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)input.data(),input.size(),digest);
    //转成十六进制字符串
    std::ostringstream oss;
    for(int i=0;i<MD5_DIGEST_LENGTH;i++){
        oss<<hex<<setw(2)<<setfill('0')<<(int)digest[i];
    }
    return oss.str();
}

/* redis处理模块，写入user hash 
key: user:username:<username>
kvs:vector of{field,value}
*/
bool write_user_to_redis(redisContext * rconn,const string &username,const vector<pair<string,string>>&kvs,int expire_seconds=3600){
    if(!rconn)
        return false;
    string key="user:username:"+username;
    //逐项hset
    for(const auto &p:kvs){
        redisReply * rep=(redisReply*)redisCommand(rconn,"HSET %s %s %s",key.c_str(),p.first.c_str(),p.second.c_str());
        if(!rep){
            return false;
        }
        freeReplyObject(rep);
    }
    //设置过期
    if(expire_seconds>0){
        redisReply *r2=(redisReply *)redisCommand(rconn,"EXPIRE %s %d",key.c_str(),expire_seconds);
        if(!r2)
            return false;
        freeReplyObject(r2);
    }
    return true;
}

/* -------------------redis helper:读取user hash--------------------
返回true 表示命中，并填充out_kv(field,value)
*/
bool read_user_from_redis(redisContext * rconn,const string &username,vector<pair<string,string>>& out_kv){
    if(!rconn)
        return false;
    string key="user:username:"+username;
    redisReply *reply=(redisReply*)redisCommand(rconn,"HGETALL %s",key.c_str());
    if(!reply)
        return false;
    if(reply->type==REDIS_REPLY_ARRAY&&reply->elements>0){
        for(size_t i=0;i+1<reply->elements;i+=2){
            string field(reply->element[i]->str,reply->element[i]->len);
            string val(reply->element[i+1]->str,reply->element[i+1]->len);
            out_kv.emplace_back(field,val);
        }
        freeReplyObject(reply);
        return true;
    }
    freeReplyObject(reply);
    return false;
}
/*------------------mysql处理模块，从数据库中查询，并且使用了mysql——real——escape——string防注入*/
bool read_user_from_mysql_by_username(MYSQL*mconn,const string &username,vector<pair<string,string>>&out_kv){
    if(!mconn)
        return false;
    //安全处理,转义username
    char esc[256];
    unsigned long len=username.size();
    string username_escaped;
    username_escaped.resize(len*2+1);
    unsigned long new_len=mysql_real_escape_string(mconn,(char*)username_escaped.data(),username.c_str(),len);
    username_escaped.resize(new_len);
    string sql="select id,username,password,email,age from users where username='"+username_escaped+"' limit 1";
    if(mysql_query(mconn,sql.c_str())){
        cerr<<"mysql_query error:"<<mysql_error(mconn)<<endl;
        return false;
    }
    MYSQL_RES * res=mysql_store_result(mconn);
    if(!res)
        return false;
    MYSQL_ROW row =mysql_fetch_row(res);
    if(!row){
        mysql_free_result(res);
        return false;
    }

    out_kv.emplace_back("id",row[0]?row[0]:"");
    out_kv.emplace_back("username",row[1]?row[1]:"");
    out_kv.emplace_back("password",row[2]?row[2]:"");
    out_kv.emplace_back("email",row[3]?row[3]:"");
    out_kv.emplace_back("age",row[4]?row[4]:"");
    mysql_free_result(res);
    return true;//
}
/* 注册用户 */
bool register_user(const string&username,const string&password,redisContext *rconn){//这里rconn做什么的
    Mysql_Connect_Pool * pool=Mysql_Connect_Pool::getinstance();
    MYSQL *mconn=pool->getconnection();
    if(!mconn){
        cerr<<"数据库无法连接"<<endl;
        return false;
    }
    //检查有无冲突
    vector<pair<string,string>>tmp;
    bool exists=read_user_from_mysql_by_username(mconn,username,tmp);//tmp具体是什么内容
    if(exists){
        pool->returnconnection(mconn);
        cout<<"注册失败，用户已存在！"<<endl;
        return false;
    }
    //新用户插入
    string pwd_hash=md5_hex(password);
    //使用 mysql_real_escape_string 转义字段
    string username_esc;
    username_esc.resize(username.size()*2+1);
    unsigned long usernamelen=mysql_real_escape_string(mconn,(char *)username_esc.data(),username.c_str(),username.size());
    username_esc.resize(usernamelen);//前面和这里的resize到底是为了啥

    string pwd_esc;
    pwd_esc.resize(pwd_hash.size()*2+1);
    unsigned long passwordlen=mysql_real_escape_string(mconn,(char *)pwd_esc.data(),pwd_hash.c_str(),pwd_hash.size());
    pwd_esc.resize(passwordlen);

    string insert_sql="insert into users(username,password) values('"+username_esc+"','"+pwd_esc+"')";
    if(mysql_query(mconn,insert_sql.c_str())){
        cerr<<"插入失败:"<<mysql_error(mconn)<<endl;
        pool->returnconnection(mconn);
        return false;
    }

    //获取新插入一行的id
    unsigned long long new_id=mysql_insert_id(mconn);
    //写入redis缓存
    vector<pair<string,string>>kvs;//kvs这个缩写到底有何含义
    kvs.emplace_back("id",to_string(new_id));
    kvs.emplace_back("username",username);
    kvs.emplace_back("password",pwd_hash);//这里为啥不用加密？
    //暂时先写这么多字段
    bool ok=write_user_to_redis(rconn,username,kvs,3600);
    pool->returnconnection(mconn);
    return ok;
}
/* 用户登录（缓存优先） 
具体流程： 先从redis查询，若命中则直接把redis中的密码与登录密码比较
若未命中，则查mysql，并把用户写入redis，再比对密码

*/
bool login_user(const string&username,const string &password,redisContext * rconn){
    //查redis
    vector<pair<string,string>>kvs;
    bool hit=read_user_from_redis(rconn,username,kvs);
    string stored_password_hash;
    if(hit){
        for(auto &p:kvs){
            if(p.first=="password")
                stored_password_hash=p.second;
        }
        if(stored_password_hash.empty()){
            cerr<<"缓存密码为空？登陆错误"<<endl;
        }else{
            string input_password_hash=md5_hex(password);
            if(input_password_hash==stored_password_hash)
                return true;
            else
                return false;
        }
    }
    //缓存未命中时
    Mysql_Connect_Pool*pool=Mysql_Connect_Pool::getinstance();
    MYSQL * mconn=pool->getconnection();
    if(!mconn){
        cerr<<"缓存无法匹配，无法获取mysql连接"<<endl;
        return false;
    }
    vector<pair<string,string>>mysql_kv;
    bool found=read_user_from_mysql_by_username(mconn,username,mysql_kv);
    pool->returnconnection(mconn);
    if(!found)
        return false;

    //再把查到的结果写入redis
    write_user_to_redis(rconn,username,mysql_kv,3600);

    //比较密码
    string database_password;
    for(auto &p:mysql_kv){
        if(p.first=="password"){
            database_password=p.second;
        }
    }
    string provided_hash=md5_hex(password);//这里怎么又搞一个变量？
    return (provided_hash==database_password);
}