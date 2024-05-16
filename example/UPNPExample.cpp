#include <upnp.hpp>
#include <iostream>


int main() {
    auto upnp = std::make_shared<sgns::upnp::UPNP>();
    auto gotIGD = upnp->GetIGD();
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