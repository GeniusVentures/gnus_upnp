#include <upnp.hpp>
#include <iostream>


int main() {
    // Your program logic here
    auto upnp = std::make_shared<sgns::upnp::UPNP>(1, 1);
    upnp->GetIGD();
    return 0; // Return 0 to indicate successful program execution
}