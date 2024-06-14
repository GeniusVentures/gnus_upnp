#include <upnp.hpp>
#include <iostream>
#include <boost/asio/error.hpp>


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
    auto upnp = std::make_shared<sgns::upnp::UPNP>();
    auto gotIGD = upnp->GetIGD();
    std::cout << "Got IGD:" << gotIGD << std::endl;
    if (gotIGD)
    {
        auto openedPort = upnp->OpenPort(7777, 7777, "TCP", 60);
        auto wanip = upnp->GetWanIP();
        auto lanip = upnp->GetLocalIP();
        std::cout << "Wan IP: " << wanip << std::endl;
        std::cout << "Lan IP: " << lanip << std::endl;
        if (!openedPort)
        {
            std::cerr << "Failed to open port" << std::endl;
        }
        else {
            std::cout << "Open Port Success" << std::endl;
        }
    }


    return 0; 
}