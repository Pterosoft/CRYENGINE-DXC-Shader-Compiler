# CRYENGINE DXC Shader Compiler
This code will allow you to compile .hlsl shaders inside CRYENGINE. By default the engine uses FXC and this will add DXC into the engine. This was part of another project that will add DXR into CRYENGINE.
## Requirements
You need CRYENGINE 5.7 source code. The file structure already copies the file structure of the engine so you can drag and drop the files there. In the project the files should be located/imported in CryRenderD3D11 and CryRenderD3D12 in Source Files (.cpp) and in Header Files (.h). Besides the required SDKs listed in the CRYENGINE source readme file you will also need DirectX SDK.
## Note
The code was built as part of hardware accelerated ray tracing for CRYENGINE. Using it for other purposes has not been tested.