#pragma once

namespace RendererTrace::OverlayDrawValidation {
// Read only the requested texture from the installed game archive.
inline ComPtr<IDirect3DTexture9> LoadOverlayTexture(IDirect3DDevice9* device,const std::string& name) {
    if(name.empty() || name.size()>15 || name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos)
        throw std::runtime_error("Invalid overlay resource name.");
    std::ifstream file("data01/sprites.gfs",std::ios::binary|std::ios::ate);
    const auto length=file.tellg();
    if(length<51)throw std::runtime_error("Could not open the game sprite archive.");
    const auto archiveSize=static_cast<uint64_t>(length);
    auto read=[&](void* buffer,size_t size) {
        if(!file.read(static_cast<char*>(buffer),size))throw std::runtime_error("Truncated sprite archive.");
    };
    auto bigEndian=[](const unsigned char* bytes,size_t count) {
        uint64_t value=0;
        for(size_t i=0;i<count;i++)value=(value<<8)|bytes[i];
        return value;
    };
    unsigned char header[51];
    file.seekg(0);
    read(header,sizeof(header));
    const auto dataStart=bigEndian(header,4), entries=bigEndian(header+43,8);
    if(dataStart<51 || dataStart>archiveSize || dataStart>64*1024*1024 || entries>1000000)
        throw std::runtime_error("Invalid sprite archive header.");
    std::vector<unsigned char> metadata(static_cast<size_t>(dataStart-51));
    read(metadata.data(),metadata.size());
    const auto wanted="sprites/chowder_"+name+".dds";
    uint64_t offset=dataStart;
    size_t cursor=0;
    for(uint64_t i=0;i<entries;i++) {
        if(metadata.size()-cursor<20)throw std::runtime_error("Truncated sprite archive entry.");
        const auto nameLength=bigEndian(metadata.data()+cursor,8);
        cursor+=8;
        if(nameLength>metadata.size()-cursor-12)throw std::runtime_error("Invalid sprite archive entry name.");
        std::string_view entry(reinterpret_cast<const char*>(metadata.data()+cursor),static_cast<size_t>(nameLength));
        cursor+=static_cast<size_t>(nameLength);
        const auto size=bigEndian(metadata.data()+cursor,8);
        cursor+=12;
        if(size>archiveSize-offset)throw std::runtime_error("Invalid sprite archive resource size.");
        if(entry!=wanted){offset+=size;continue;}
        if(size<128 || size>16*1024*1024)throw std::runtime_error("Unsupported overlay texture size.");
        std::vector<unsigned char> dds(static_cast<size_t>(size));
        file.seekg(static_cast<std::streamoff>(offset));
        read(dds.data(),dds.size());
        const auto width=Word(dds,16),height=Word(dds,12);
        if(Word(dds,0)!=0x20534444 || Word(dds,4)!=124 || Word(dds,76)!=32 ||
           Word(dds,84)!=D3DFMT_DXT5 || Word(dds,28)>1 || Word(dds,112)!=0 ||
           !width || !height || width>4096 || height>4096)
            throw std::runtime_error("Expected a single-level 2D DXT5 overlay.");
        const size_t rowBytes=((width+3)/4)*16,rows=(height+3)/4;
        if(dds.size()!=128+rowBytes*rows)throw std::runtime_error("Invalid overlay pixel data length.");
        ComPtr<IDirect3DTexture9> texture;
        if(FAILED(device->CreateTexture(width,height,1,0,D3DFMT_DXT5,D3DPOOL_MANAGED,texture.GetAddressOf(),nullptr)))
            throw std::runtime_error("Direct3D rejected the overlay texture.");
        D3DLOCKED_RECT lock{};
        if(FAILED(texture->LockRect(0,&lock,nullptr,0)))throw std::runtime_error("Could not upload overlay texture.");
        if(lock.Pitch<0 || static_cast<size_t>(lock.Pitch)<rowBytes || !lock.pBits) {
            texture->UnlockRect(0);
            throw std::runtime_error("Invalid overlay texture pitch.");
        }
        for(size_t row=0;row<rows;row++)
            memcpy(static_cast<unsigned char*>(lock.pBits)+row*lock.Pitch,dds.data()+128+row*rowBytes,rowBytes);
        if(FAILED(texture->UnlockRect(0)))throw std::runtime_error("Could not complete overlay texture upload.");
        return texture;
    }
    throw std::runtime_error("Overlay texture is absent from the installed sprite archive.");
}
}
