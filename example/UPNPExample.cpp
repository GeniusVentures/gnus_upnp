#include <upnp.hpp>
#include <iostream>


int main() {
    auto upnp = std::make_shared<sgns::upnp::UPNP>();
    upnp->GetIGD();
    upnp->OpenPort(7777, 7777, "TCP");
    upnp->GetWanIP();
    return 0; 
}