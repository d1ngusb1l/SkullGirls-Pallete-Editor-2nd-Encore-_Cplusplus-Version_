#pragma once
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include "../../../PalleteEditor/Data/AtomicFile.h"
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"ole32.lib")
namespace SpritePng {
inline void Require(HRESULT result,const char* message){if(FAILED(result))throw std::runtime_error(message);}
struct Apartment {
    HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    Apartment(){if(FAILED(result) && result!=RPC_E_CHANGED_MODE)throw std::runtime_error("Could not initialize the PNG encoder.");}
    ~Apartment(){if(SUCCEEDED(result))CoUninitialize();}
};
inline std::vector<unsigned char> Encode(unsigned width,unsigned height,const std::vector<uint32_t>& bgra) {
    if(!width || !height || width>2048 || height>2048 || bgra.size()!=size_t(width)*height)throw std::runtime_error("Invalid PNG dimensions or pixel data.");
    Apartment apartment;
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    Require(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf())),"Could not create the PNG encoder.");
    Microsoft::WRL::ComPtr<IStream> stream;
    Require(CreateStreamOnHGlobal(nullptr,TRUE,stream.GetAddressOf()),"Could not create the PNG memory stream.");
    Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
    Require(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.GetAddressOf()),"PNG encoding is unavailable.");
    Require(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache),"Could not initialize PNG output.");
    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
    Microsoft::WRL::ComPtr<IPropertyBag2> properties;
    Require(encoder->CreateNewFrame(frame.GetAddressOf(),properties.GetAddressOf()),"Could not create the PNG frame.");
    Require(frame->Initialize(properties.Get()),"Could not initialize the PNG frame.");
    Require(frame->SetSize(width,height),"Could not set the PNG dimensions.");
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
    Require(frame->SetPixelFormat(&format),"Could not set PNG alpha format.");
    if(!IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA))throw std::runtime_error("The PNG encoder does not support transparency.");
    Require(frame->WritePixels(height,width*4,UINT(bgra.size()*4),reinterpret_cast<BYTE*>(const_cast<uint32_t*>(bgra.data()))),"Could not encode PNG pixels.");
    Require(frame->Commit(),"Could not finish the PNG frame.");
    Require(encoder->Commit(),"Could not finish the PNG file.");
    STATSTG stats{};Require(stream->Stat(&stats,STATFLAG_NONAME),"Could not read PNG output size.");
    if(stats.cbSize.QuadPart>64ull*1024*1024)throw std::runtime_error("Encoded PNG exceeds the export size limit.");
    LARGE_INTEGER beginning{};Require(stream->Seek(beginning,STREAM_SEEK_SET,nullptr),"Could not read PNG output.");
    std::vector<unsigned char> bytes(size_t(stats.cbSize.QuadPart));ULONG read=0;
    Require(stream->Read(bytes.data(),ULONG(bytes.size()),&read),"Could not read the complete PNG.");
    if(read!=bytes.size())throw std::runtime_error("Truncated PNG output.");
    return bytes;
}
inline void Save(const std::filesystem::path& path,unsigned width,unsigned height,const std::vector<uint32_t>& pixels) {
    PaletteData::AtomicWrite(path,Encode(width,height,pixels));
}
}
