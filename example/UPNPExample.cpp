#include <upnp.hpp>
#include <iostream>
#include <boost/asio/error.hpp>

void upnp_task(int thread_id) {
    auto upnp = std::make_shared<sgns::upnp::UPNP>();

    std::cout << "Thread " << thread_id << " calling GetIGD()" << std::endl;
    auto gotIGD = upnp->GetIGD();
    std::cout << "Thread " << thread_id << " Got IGD: " << gotIGD << std::endl;

    if (gotIGD) {
        auto openedPort = upnp->OpenPort(7477 + thread_id, 7477 + thread_id, "TCP", 60);
        auto wanip = upnp->GetWanIP();
        auto lanip = upnp->GetLocalIP();
        
        std::cout << "Thread " << thread_id << " Wan IP: " << wanip << std::endl;
        std::cout << "Thread " << thread_id << " Lan IP: " << lanip << std::endl;

        if (!openedPort) {
            std::cerr << "Thread " << thread_id << " Failed to open port" << std::endl;
        } else {
            std::cout << "Thread " << thread_id << " Open Port Success" << 7477 + thread_id << std::endl;
        }
    }
}

int main() {
    //boost::asio::io_context io_context;
    //boost::asio::ip::tcp::resolver resolver(io_context);
    //boost::asio::ip::tcp::resolver::query query(boost::asio::ip::host_name(), "");

    //boost::asio::ip::tcp::resolver::iterator it = resolver.resolve(query);

    //while (it != boost::asio::ip::tcp::resolver::iterator())
    //{
    //    boost::asio::ip::address addr = (it++)->endpoint().address();

    //    std::cout << addr.to_string() << std::endl;
    //}
    //boost::asio::ip::udp::resolver udpresolver(io_context);
    //boost::asio::ip::udp::resolver::query query2(boost::asio::ip::host_name(), "");
    //boost::asio::ip::udp::resolver::iterator itudp = udpresolver.resolve(query2);
    //while (itudp != boost::asio::ip::udp::resolver::iterator())
    //{
    //    boost::asio::ip::address addr = (itudp++)->endpoint().address();

    //    std::cout << addr.to_string() << std::endl;
    //}
    // auto upnp = std::make_shared<sgns::upnp::UPNP>();
    // auto gotIGD = upnp->GetIGD();
    // std::cout << "Got IGD:" << gotIGD << std::endl;
    // if (gotIGD)
    // {
    //     auto openedPort = upnp->OpenPort(7777, 7777, "TCP", 60);
    //     auto wanip = upnp->GetWanIP();
    //     auto lanip = upnp->GetLocalIP();
    //     std::cout << "Wan IP: " << wanip << std::endl;
    //     std::cout << "Lan IP: " << lanip << std::endl;
    //     if (!openedPort)
    //     {
    //         std::cerr << "Failed to open port" << std::endl;
    //     }
    //     else {
    //         std::cout << "Open Port Success" << std::endl;
    //     }
    // }
    const int num_threads = 5;  // Adjust as needed
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        // Introduce a slight delay to better simulate real-world concurrency
        std::this_thread::sleep_for(std::chrono::milliseconds(100 * i));
        threads.emplace_back(upnp_task, i);
    }

    for (auto& t : threads) {
        t.join();
    }


    return 0; 
}