// All Chowder DDS resources in build 26588330's sprites.gfs. Pixel data stays
// in the installed game; metadata has no session pointers or copied cache flags.
inline void BundledOverlayTemplates() {
    for(const char* name:{"BlueSpace","bunny_fishnet","Fire","Fire_large","krainbow","Space","Space_large",
                         "umbrellawater","umbrella_crack","umbrella_veins","Water","Water_large"}) {
        std::string label=name; Bytes record(120);
        memcpy(record.data()+4,label.data(),label.size());
        Put(record,20,(DWORD)label.size()); Put(record,24,15);
        // Native case-insensitive DJB2-XOR matches shipped Fire/Water/Space keys.
        DWORD key=5381;
        for(unsigned char ch:label) { if(ch>='a' && ch<='z')ch-=32; key=(key*33)^ch; }
        Put(record,28,key); record[46]=255;
        Put(record,76,15); Put(record,80,5381); Put(record,116,15);
        templates.emplace(label,record);
    }
}
