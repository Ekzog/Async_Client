#define _CRT_SECURE_NO_WARNINGS
#include <ctime>

#include <fstream>
#include <boost/json.hpp>
#include <boost/json/src.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
//#include <boost/json.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <iostream>

#include <boost/log/sinks/unlocked_frontend.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

using namespace boost::asio;
io_service service;

#define MEM_FN(x)       boost::bind(&self_type::x, shared_from_this())
#define MEM_FN1(x,y)    boost::bind(&self_type::x, shared_from_this(),y)
#define MEM_FN2(x,y,z)  boost::bind(&self_type::x, shared_from_this(),y,z)

inline std::string getCurrentDateTime(std::string s) {
    time_t now = time(0);
    struct tm  tstruct;
    char  buf[80];
    tstruct = *localtime(&now);
    if (s == "now")
        strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
    else if (s == "date")
        strftime(buf, sizeof(buf), "%Y-%m-%d", &tstruct);
    return std::string(buf);
};
inline void Logger(std::string logMsg) {
    std::string filePath = getCurrentDateTime("date") + ".txt";
    std::string now = getCurrentDateTime("now");
    std::ofstream ofs(filePath.c_str(), std::ios_base::out | std::ios_base::app);
    ofs << "[" << now << "]" << '\t' << logMsg << '\n';
    ofs.close();
}

class talk_to_svr : public boost::enable_shared_from_this<talk_to_svr>
    , boost::noncopyable {
    typedef talk_to_svr self_type;
    talk_to_svr(const std::string& message)
        : sock_(service), started_(true), message_(message) {}
    void start(ip::tcp::endpoint ep) {
        //std::cout << "Client connect to server..." << std::endl;
        Logger("Client connect to server...");
        BOOST_LOG_TRIVIAL(info) << "Client connect to server...";
        sock_.async_connect(ep, MEM_FN1(on_connect, _1));
    }
public:
    typedef boost::system::error_code error_code;
    typedef boost::shared_ptr<talk_to_svr> ptr;

    static ptr start(ip::tcp::endpoint ep, const std::string& message) {
        ptr new_(new talk_to_svr(message));
        new_->start(ep);
        return new_;
    }
    void stop() {
        if (!started_) return;
        started_ = false;
        Logger("Disconnect with server");
        BOOST_LOG_TRIVIAL(info) << "Disconnect with server";
        //std::cout << "Disconnect with server" << std::endl;
        sock_.close();
    }
    bool started() { 
        Logger("Connection to server complete!");
        BOOST_LOG_TRIVIAL(info) << "Connection to server complete!";
        //std::cout << "Connection to server complete!" << std::endl; 
        return started_;
    }
private:
    void on_connect(const error_code& err) {
        if (!err) {
            do_write(message_ + "\n");
        }
        else {
            Logger("Connection error");
            BOOST_LOG_TRIVIAL(info) << "Connection error: " << err;
            //std::cout << "Connection error: " << err << std::endl;
            stop();
        }
    }
    void on_read(const error_code& err, size_t bytes) {
        if (!err) {
            std::string copy(read_buffer_, bytes - 1);
            Logger("Connection error: " + copy);
            BOOST_LOG_TRIVIAL(info) << "Read json from server: " << copy;
            //std::cout << "Read json from server: " << copy << std::endl;
            //std::cout << "server echoed our " << copy;
            std::stringstream jsonEncodedData(copy);
            boost::property_tree::ptree rootHive;
            boost::property_tree::read_json(jsonEncodedData, rootHive);
            try
            {
                Logger("Server sent answer");
                BOOST_LOG_TRIVIAL(info) << "Server sent answer: " << rootHive.get<double>("answer");
                //std::cout << "Server sent answer: " << rootHive.get<double>("answer") << std::endl;
            }
            catch (...)
            {
                Logger("Server sent answer");
                BOOST_LOG_TRIVIAL(info) << "Server sent answer: " << rootHive.get<std::string>("answer");
                //std::cout << "Server sent answer: " << rootHive.get<std::string>("answer") << std::endl;
            }
        }
        else std::cout << "Error of reading: " << err << std::endl;
        stop();
    }

    void on_write(const error_code& err, size_t bytes) {
        do_read();
    }
    void do_read() {
        Logger("Getting json from server...");
        BOOST_LOG_TRIVIAL(info) << "Getting json from server...";
        //std::cout << "Getting json from server..." << std::endl;
        async_read(sock_, buffer(read_buffer_),
            MEM_FN2(read_complete, _1, _2), MEM_FN2(on_read, _1, _2));
    }
    void do_write(const std::string& msg) {
        if (!started()) return;
        Logger("Loading json to buffer: " + message_);
        BOOST_LOG_TRIVIAL(info) << "Loading json to buffer: " << message_;
        //std::cout << "Loading json to buffer: " << message_ << std::endl;
        std::copy(msg.begin(), msg.end(), write_buffer_);

        Logger("Sending json to server... ");
        BOOST_LOG_TRIVIAL(info) << "Sending json to server... ";
        //std::cout << "Sending json to server... " << std::endl;
        sock_.async_write_some(buffer(write_buffer_, msg.size()),
            MEM_FN2(on_write, _1, _2));
        Logger("Sending complete!");
        BOOST_LOG_TRIVIAL(info) << "Sending complete!";
        //std::cout << "Sending complete!" << std::endl;
    }
    size_t read_complete(const boost::system::error_code& err, size_t bytes) {
        if (err) {
            Logger("Reading error");
            BOOST_LOG_TRIVIAL(info) << "Reading error: " << err;
            //std::cout << "Reading error: " << err << std::endl;
            return 0;
        }
        bool found = std::find(read_buffer_, read_buffer_ + bytes, '\n') < read_buffer_ + bytes;
        // we read one-by-one until we get to enter, no buffering
        return found ? 0 : 1;
    }

private:
    ip::tcp::socket sock_;
    enum { max_msg = 1024 };
    char read_buffer_[max_msg];
    char write_buffer_[max_msg];
    bool started_;
    std::string message_;
};



int main(int argc, char* argv[]) {
    // connect several clients
    ip::tcp::endpoint ep(ip::address::from_string("127.0.0.1"), 8001);
    double X;
    std::cout << "X: ";
    std::cin >> X;

    std::string opperator;
    std::cout << "Operator: ";
    std::cin >> opperator;

    double Y;
    std::cout << "Y: ";
    std::cin >> Y;

    /////////////////////////////////
    std::string myString = "{}";
    std::stringstream jsonEncodedData(myString);
    boost::property_tree::ptree rootHive;
    rootHive.put("equation.X", X);
    rootHive.put("equation.operator", opperator);
    rootHive.put("equation.Y", Y);

    boost::property_tree::write_json(jsonEncodedData, rootHive);
    boost::json::error_code errorCode;
    auto jsonValue = boost::json::parse(jsonEncodedData, errorCode);
    std::ostringstream osstr;
    osstr << jsonValue;
    std::string request = osstr.str();
    talk_to_svr::start(ep, request);
    boost::this_thread::sleep(boost::posix_time::millisec(100));
    service.run();
}
