This is a boost 1.80.0 based upnp implementation

# Download gnus_upnp project

    git clone git@github.com:GeniusVentures/gnus_upnp.git --recursive 
    cd SuperGenius
    git checkout develop
    
# Download thirdparty project

    cd ..
    git clone git@github.com:GeniusVentures/thirdparty.git --recursive 
    cd thirdparty
    git checkout develop

# Build example on Windows

    1. cd gnus_upnp/build/Windows
    2. mkdir Release
    3. cd Release
    4. cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DTHIRDPARTY_DIR=[ABSOLUTE_PATH_TO_THIRDPARTY_BUILD_RELEASE]
    5. cmake --build . --parallel 8 --config Release
    
# Build example on Linux

# Build example on OSX
