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
        std::string owner;
        if (upnp->CheckIfPortInUse(31129, "UDP", owner)) {
            if (owner != upnp->GetLocalIP()) {
                std::cout << "Port is already mapped by another LAN IP: " << owner << std::endl;
            }
            else {
                std::cout << "Port is already mapped by this device." << std::endl;
            }
        }
        else {
            std::cout << "Port is free for mapping." << std::endl;
        }
    }


}

int main() {
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