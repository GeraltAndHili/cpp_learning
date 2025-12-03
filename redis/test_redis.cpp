#include<hiredis/hiredis.h>
#include<iostream>
using namespace std;




int main(){
    redisContext * rconn=redisConnect("127.0.0.1",6379);
    if(!rconn||rconn->err){
        cerr<<"redis连接失败:"<<(rconn?rconn->errstr:"rconn is null")<<endl;
        if(rconn)
        redisFree(rconn);
        return -1;
    }
    cout<<"redis连接成功\n";

    //设置key
    redisReply * reply=(redisReply*)redisCommand(rconn,"set %s %s","demo:key","hello_from_cpp");
    if(!reply){
        cerr<<"set 命令失败\n";
        redisFree(rconn);
        return-1;
    }
    freeReplyObject(reply);

    //获取key
    reply =(redisReply*) redisCommand(rconn,"get %s","demo:key");
    if(!reply){
        cerr<<"get 命令失败\n"<<endl;
        redisFree(rconn);
        return -1;
    }else {
        cout<<"get demo:key 返回类型="<<reply->type<<endl;
    }
    freeReplyObject(reply);

    reply = (redisReply*)redisCommand(rconn, "HSET user:1 id %s name %s", "1", "Alice");
    freeReplyObject(reply);
    reply = (redisReply*)redisCommand(rconn, "HGETALL %s", "user:1");
    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        std::cout << "HGETALL user:1 返回 " << reply->elements << " 个元素 (key/value 对数为 elements/2)" << std::endl;
        for (size_t i = 0; i < reply->elements; ++i)
            std::cout << reply->element[i]->str << (i % 2 ? "\n" : " : ");
    }
    if (reply) freeReplyObject(reply);

    // 5. 释放连接
    redisFree(rconn);

    return 0;
}