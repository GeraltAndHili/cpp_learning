#include<iostream>
#include<sstream>
#include<string>
#include<vector>
#include<hiredis/hiredis.h>
#include"ConnectionPool.h"
#include"connectionpool_declare.h"
using namespace std;

/* 具体功能：
1、从redis读取user:<id>的hash
2、如果缓存未命中，则从mysql查user，取出若干字段，写入redis并设置过期时间
3、再次读取redis并打印，确认缓存写入成功
 */

 //首先从redis读取user的hash
bool read_user_from_redis(redisContext * rconn,int user_id,vector<pair<string,string>>& out_kv){//这里out_kv是什么
    if(!rconn)
        return false;
    string key="user:"+to_string(user_id);
    redisReply * reply=(redisReply*) redisCommand(rconn,"HGETALL %s",key.c_str());//这里变量reply类型和右边的等式具体是什么意思？参数又是什么意思
    if(!reply){
        cerr<<"[redis]HGETALL命令失败或者连接已断开"<<endl;
        return false;
    }
    bool found=false;
    if (reply->type == REDIS_REPLY_ARRAY && reply->elements > 0) {
        // elements 是数组元素数（应该为偶数：field,value,field,value,...）
        for (size_t i = 0; i + 1 < reply->elements; i += 2) {
            // 每个 element[i] 是 redisReply*，里面的 str/len 可用于构造 string（防止二进制问题）
            string field(reply->element[i]->str, reply->element[i]->len);
            string val(reply->element[i+1]->str, reply->element[i+1]->len);
            out_kv.emplace_back(field, val);
        }
        found = true; // 读取到数据
    }

    freeReplyObject(reply);
    return found;
}
bool write_user_to_redis(redisContext * rconn,int user_id,const vector<pair<string,string>>&kvs,int expire_seconds=3600){//这里 vector存储的是什么内容
    if(!rconn)
        return false;
    string key="user:"+to_string(user_id);

        for (const auto& p : kvs) {
        // p.first = field, p.second = value
        redisReply* rep = (redisReply*)redisCommand(rconn, "HSET %s %s %s", key.c_str(), p.first.c_str(), p.second.c_str());
        if (!rep) {
            cerr << "[Redis] HSET 失败（可能连接断开）" << endl;
            return false; // 注意：未 freeReplyObject(rep) 因为 rep 为 nullptr
        }
        freeReplyObject(rep);
    }

    // 设置过期时间（如果需要）
    if (expire_seconds > 0) {
        redisReply* r2 = (redisReply*)redisCommand(rconn, "EXPIRE %s %d", key.c_str(), expire_seconds);
        if (!r2) {
            cerr << "[Redis] EXPIRE 命令失败" << endl;
            return false;
        }
        freeReplyObject(r2);
    }
    return true;
}

bool read_user_from_mysql(MYSQL *mconn,int user_id,vector<pair<string,string>>& out_kv){
    if(!mconn){
        return false;
    }

    string sql="select id,username,age from users where id="+to_string(user_id)+" LIMIT 1";

    if(mysql_query(mconn,sql.c_str())){
        cerr<<"[mysql]查询失败"<<mysql_error(mconn)<<endl;
        return false;
    }
    MYSQL_RES *res=mysql_store_result(mconn);//这里变量res是什么类型，什么作用
    if(!res){
        cerr<<"[MYSQL]mysql_store_result返回nullptr(没有结果或者出错):"<<mysql_error(mconn)<<endl;
        return false;
    }
    //只取第一行
    MYSQL_ROW row=mysql_fetch_row(res);//这里的row是什么类型
    if(!row){
        mysql_free_result(res);
        return false;
    }

    //这里row时char*类型数组，对应id，username，age
    out_kv.emplace_back("id",row[0]?row[0]:"");
    out_kv.emplace_back("username",row[1]?row[1]:"");
    out_kv.emplace_back("age",row[2]?row[2]:"");   
    mysql_free_result(res);
    return true;
}
int main(){
    int user_id=5;
    int expire_seconds=15;
    cout<<"-----cache_user_demo 启动-----"<<endl;

    redisContext * rconn=redisConnect("127.0.0.1",6379);//为什么是6379
    if(!rconn){
        cerr<<"[error]redisconnect返回null"<<endl;
        return -1;
    }
    if(rconn->err){
        cerr<<"[error]无法连接redis"<<rconn->errstr<<endl;
        redisFree(rconn);
        return -1;
    }
    cout<<"redis连接成功"<<endl;

    //尝试读取缓存
    vector<pair<string,string>>kvs;
    bool hit=read_user_from_redis(rconn,user_id,kvs);

    if(hit){
        cout<<"从redis中读到user,id为:"<<user_id<<endl;
        for(auto &p:kvs){
            cout<<"  "<<p.first<<"="<<p.second<<endl;
        }
    }else{
        //缓存未命中
        cout<<"redis不存在user_id为"<<user_id<<"的数据，将从mysql查询并写入redis"<<endl;
        Mysql_Connect_Pool * pool=Mysql_Connect_Pool::getinstance();
        MYSQL*mconn=pool->getconnection();
        if(!mconn){
            cerr<<"无法连接到mysql"<<endl;
            redisFree(rconn);
            return -1;
        }
        vector<pair<string,string>>mysql_kv;
        bool found=read_user_from_mysql(mconn,user_id,mysql_kv);

        pool->returnconnection(mconn);
        if(!found){
            cout<<"mysql表中也没找到id="<<user_id<<"的数据"<<endl;
            redisFree(rconn);
            return 0;
        }
        cout<<"查询到用户，写入redis(expire)"<<expire_seconds<<"s"<<endl;
        bool ok=write_user_to_redis(rconn,user_id,mysql_kv,expire_seconds);
        if(!ok){
            cerr<<"redis写入失败"<<endl;
            redisFree(rconn);
            return -1;
        }
        kvs.clear();
        bool hit2=read_user_from_redis(rconn,user_id,kvs);
        if(hit2){
            cout<<"现在成功从redis读取到user："<<user_id<<endl;
            for(auto & p:kvs)cout<<" "<<p.first<<"="<<p.second<<endl;        
        }else{
            cerr<<"尝试从mysql写入后读取redis仍然失败"<<endl;
        }
    }
    redisFree(rconn);
    cout<<"------程序结束------"<<endl;
    cout<<"写入缓存的数据20秒后失效,你可以在"<<expire_seconds<<"后再次尝试并观察"<<endl;
    return 0;
}