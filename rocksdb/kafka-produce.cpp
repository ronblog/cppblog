#include <iostream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <cstring>
 
//#include <getopt.h>
#include <D:\3rdLib\librdkafka\src-cpp\rdkafkacpp.h>
 
static bool run = true;
 
static void sigterm (int sig) {
    run = false;
}
 
class ExampleDeliveryReportCb : public RdKafka::DeliveryReportCb {
public:
    void dr_cb (RdKafka::Message &message) {
        std::cout << "Message delivery for (" << message.len() << " bytes): " <<
                     message.errstr() << std::endl;
        if (message.key())
            std::cout << "Key: " << *(message.key()) << ";" << std::endl;
    }
};
 
class ExampleEventCb : public RdKafka::EventCb {
public:
    void event_cb (RdKafka::Event &event) {
        switch (event.type())
        {
        case RdKafka::Event::EVENT_ERROR:
            std::cerr << "ERROR (" << RdKafka::err2str(event.err()) << "): " <<
                         event.str() << std::endl;
            if (event.err() == RdKafka::ERR__ALL_BROKERS_DOWN)
                run = false;
            break;
 
        case RdKafka::Event::EVENT_STATS:
            std::cerr << "\"STATS\": " << event.str() << std::endl;
            break;
 
        case RdKafka::Event::EVENT_LOG:
            fprintf(stderr, "LOG-%i-%s: %s\n",
                    event.severity(), event.fac().c_str(), event.str().c_str());
            break;
 
        default:
            std::cerr << "EVENT " << event.type() <<
                         " (" << RdKafka::err2str(event.err()) << "): " <<
                         event.str() << std::endl;
            break;
        }
    }
};
 
int main ()
{
    std::string brokers = "localhost";
    std::string errstr;
    std::string topic_str="test";
    int32_t partition = RdKafka::Topic::PARTITION_UA;
 
    RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    RdKafka::Conf *tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);
 
    conf->set("bootstrap.servers", brokers, errstr);
 
    ExampleEventCb ex_event_cb;
    conf->set("event_cb", &ex_event_cb, errstr);
 
    signal(SIGINT, sigterm);
    signal(SIGTERM, sigterm);
 
    ExampleDeliveryReportCb ex_dr_cb;
    conf->set("dr_cb", &ex_dr_cb, errstr);
 
    RdKafka::Producer *producer = RdKafka::Producer::create(conf, errstr);
    if (!producer) {
        std::cerr << "Failed to create producer: " << errstr << std::endl;
        exit(1);
    }
    std::cout << "% Created producer " << producer->name() << std::endl;
 
    RdKafka::Topic *topic = RdKafka::Topic::create(producer, topic_str,
                                                   tconf, errstr);
    if (!topic) {
        std::cerr << "Failed to create topic: " << errstr << std::endl;
        exit(1);
    }
 
    for (std::string line; run && std::getline(std::cin, line);) {
        if (line.empty()) {
            producer->poll(0);
            continue;
        }
 
        RdKafka::ErrorCode resp =
                producer->produce(topic, partition,
                                  RdKafka::Producer::RK_MSG_COPY /* Copy payload */,
                                  const_cast<char *>(line.c_str()), line.size(),
                                  NULL, NULL);
        if (resp != RdKafka::ERR_NO_ERROR)
            std::cerr << "% Produce failed: " <<
                         RdKafka::err2str(resp) << std::endl;
        else
            std::cerr << "% Produced message (" << line.size() << " bytes)" <<
                         std::endl;
 
        producer->poll(0);
    }
 
    run = true;
   // 退出前处理完输出队列中的消息
    while (run && producer->outq_len() > 0) {
        std::cerr << "Waiting for " << producer->outq_len() << std::endl;
        producer->poll(1000);
    }
 
    delete conf;
    delete tconf;
    delete topic;
    delete producer;
 
    RdKafka::wait_destroyed(5000);
 
    return 0;
}