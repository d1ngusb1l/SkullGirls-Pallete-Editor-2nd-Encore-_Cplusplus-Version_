// Included inside OverlayEditor. Build 26588330's resource API takes a native
// MSVC x86 string in EDX and returns a shared asset pair through ECX. Resolve
// sprites/chowder_<name>.dds before attaching an overlay, rather than treating
// the preset catalog as evidence that the renderer can load its pixels.
inline void ResolveOverlayTexture(const std::string& name) {
    if(resolvedOverlayTextures.count(name)) return;
    const std::string path="sprites/chowder_"+name+".dds";
    try {
        if(name.empty() || name.size()>15 || name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos)
            throw std::runtime_error("Invalid resource name.");
        DWORD asset=0;
#ifdef SGPE_DLL_HOST
        // Keep one native shared-asset lease per name until game exit, just as
        // detached palette buffers are retained. The render thread subsequently
        // creates and owns the GPU texture through its normal Chowder cache.
        DWORD result[2]{};
        Bytes nativeString(24); Put(nativeString,0,(DWORD)path.c_str());
        Put(nativeString,16,(DWORD)path.size()); Put(nativeString,20,(DWORD)path.size());
        auto load=(void* (__fastcall*)(DWORD*,const void*))(base+0x11c590);
        load(result,nativeString.data()); asset=result[0];
#else
        // The standalone host uses the same native API in the target process.
        // Do not suspend it: resource loading may wait for the game's workers.
        void* remote=VirtualAllocEx(process,nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
        if(!remote) throw std::runtime_error("Could not allocate a resource request.");
        DWORD address=(DWORD)remote;
        Bytes request(128); Put(request,16,address+64); Put(request,32,(DWORD)path.size()); Put(request,36,(DWORD)path.size());
        memcpy(request.data()+64,path.c_str(),path.size()+1);
        // mov ecx,result; mov edx,string; mov eax,load; call eax;
        // mov eax,[result]; ret 4 (LPTHREAD_START_ROUTINE).
        Bytes code{0xb9,0,0,0,0,0xba,0,0,0,0,0xb8,0,0,0,0,0xff,0xd0,0xa1,0,0,0,0,0xc2,4,0};
        Put(code,1,address); Put(code,6,address+16); Put(code,11,base+0x11c590); Put(code,18,address);
        HANDLE thread=nullptr; DWORD wait=WAIT_FAILED;
        try {
            Write(address,request); Write(address+256,code); FlushInstructionCache(process,(void*)(address+256),code.size());
            thread=CreateRemoteThread(process,nullptr,0,(LPTHREAD_START_ROUTINE)(address+256),nullptr,0,nullptr);
            if(!thread) throw std::runtime_error("Could not start the resource request.");
            wait=WaitForSingleObject(thread,10000);
            if(wait!=WAIT_OBJECT_0) throw std::runtime_error("Resource request did not complete.");
            asset=Word(address);
        } catch(...) {
            if(thread) CloseHandle(thread);
            // A timed-out loader may still reference the request string/code.
            if(!thread || wait==WAIT_OBJECT_0) VirtualFreeEx(process,remote,0,MEM_RELEASE);
            throw;
        }
        CloseHandle(thread); VirtualFreeEx(process,remote,0,MEM_RELEASE);
#endif
        if(!asset) throw std::runtime_error("The installed game could not load "+path+".");
        resolvedOverlayTextures.insert(name);
    } catch(const std::exception& e) {
        throw std::runtime_error("Could not load overlay texture '"+name+"': "+e.what());
    }
}
