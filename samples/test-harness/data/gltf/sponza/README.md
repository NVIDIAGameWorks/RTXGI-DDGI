# Using Sponza in the RTXGI Test Harness

To get the Sponza rendering with RTXGI in the Test Harness, follow these steps:

1. Clone https://github.com/KhronosGroup/glTF-Sample-Models to somewhere on your machine (but don't clone it inside of this project).
2. Copy or move the contents of https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/Sponza/glTF into ```this directory```.
3. Change the Test Harness project's launch arguments to use the Sponza configuration file (```samples/test-harness/config/sponza.ini```)
   * If using Visual Studio, modify the Test Harness project's ```Command Arguments```:

        ```Project->Properties->Debugging->CommandArguments: ../../../samples/test-harness/config/sponza.ini```

   * If using Visual Studio Code, modify the ```samples/test-harness/launch.json``` file's ```args``` field: 

        ```"args": [ "../../test-harness/config/sponza.ini" ]```

4. Run the Test Harness.

## Important Note
On the first run (only), the test harness loads all scene textures, compresses them to BC7 format, generates mipmaps, and writes all scene data to a binary cache file (```Sponza.cache``` in this case). 

The texture processing steps use the [DirectXTex library](https://github.com/microsoft/DirectXTex). If on Windows, the library performs compression with the GPU and D3D11. On Linux, compression is performed entirely on the CPU and is quite slow as a result. It is recommended to generate scene cache files on Windows if possible.