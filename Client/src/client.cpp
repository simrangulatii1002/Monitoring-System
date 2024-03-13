#include <client.h>

Client* Client::instance = nullptr;

Client* Client::getInstance(){
    if(instance == nullptr) {
        instance = new Client();
        return instance;
    }
    return instance;
}

void Client::initialize(const std::string &ip, const std::string &port, const std::string &conKey) {
    serverIP_ = ip;
    port_ = port;
    ConnectionKey_ = conKey;
    shouldRun_ = true;
    reconnectAttempts_ = 0;

    ctx_ = ssl::context {ssl::context::tlsv12_server};
    ctx_.load_verify_file("/home/vboxuser/MonitoringSys/Certificates/server.crt");
    ctx_.set_verify_mode(ssl::verify_peer);
}

void Client::connect() {
    try {
        tcp::resolver resolver(ioc_);
        auto results = resolver.resolve(serverIP_, port_);

        boost::asio::connect(stream_.next_layer().next_layer(), results.begin(), results.end());

        // Handshake with SSL
        stream_.next_layer().handshake(ssl::stream_base::client);
        
        // Handshake with WebSocket
        stream_.handshake(serverIP_, "/");
    } catch (const std::exception& e) {
        std::cerr << "Connect Exception: " << e.what() << std::endl;
        throw; // Re-throw the exception to propagate it further if needed
    }
}

void Client::keyVerification() {
    // boost::system::error_code write_error;
    size_t bytes_written = stream_.write(boost::asio::buffer(ConnectionKey_), errorCode);

    if (errorCode) { 
        if (errorCode) {
            std::cerr << "Error sending connection key to the server: " << errorCode.message() << std::endl;
        } else {
            std::cout << "Sent connection key to the server \n" << std::endl;
        }
    } else {
        std::cout << "Sent connection key to the server \n" << std::endl;
    }
}

void Client::sysInfo() {
    try {
        json message_json;
        message_json["hostname"] = getHostname();
        message_json["ipAddress"] = getIPAddress();
        message_json["cpu"] = std::to_string(getCPUUsage());
        message_json["ram"] = std::to_string(getRAMUsage());
        message_json["netstate"] = getNetworkStats();

        std::string message = message_json.dump(); 
        // boost::asio::write(socket, buffer(message), errorCode);
        size_t bytes_written = stream_.write(buffer(message), errorCode);

        if(errorCode) {
            std::cerr << "Error in sending data: " << errorCode.message() << std::endl;
        }
    }
    catch(const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

void Client::receiveResponse() {
    try {
        char response[1024]; 
        size_t response_length = stream_.read_some(buffer(response), errorCode); 
        if (errorCode) { 
            std::cerr << "Error receiving message from server: " << errorCode.message() << std::endl; 
        } else { 
            std::string received_message(response, response_length);
            // Parse JSON
            json received_json = json::parse(received_message);
            std::string message = received_json["message"];
            std::cout << message<< std::endl; 
        }
    }
    catch(const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

void Client::run() {
    try
    {
        connect();
        keyVerification();
        while (shouldRun_)    //
        {
            sysInfo();
            receiveResponse();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (reconnectAttempts_ < 2 && shouldRun_==true)
        {
            reconnectAttempts_++;
            ioc_.restart();
            run();  // Reconnect and continue
        }
        else
        {
            std::cout << "Not able to connect" << std::endl;
        }
    }
}

void Client::disconnect() {
    return;
}

