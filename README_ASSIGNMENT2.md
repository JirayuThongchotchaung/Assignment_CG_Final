# Assignment 2 — FUTURE ME

โปรเจกต์นี้สร้างจาก `OpenGLStarter` (C++17 + OpenGL 3.3 Core, GLFW/GLEW/GLM/stb ผ่าน vcpkg)
วิธีติดตั้งและ build ดูใน `README.md` (เหมือนแล็บ) และเงื่อนไขงานดูใน `ASSIGNMENT2_REQUIREMENTS.md`

## โครงสร้างโฟลเดอร์
```
Assignment2/
├── ASSIGNMENT2_REQUIREMENTS.md   เงื่อนไขงาน + checklist
├── CMakeLists.txt                target ชื่อ Assignment2
├── src/main.cpp                  โค้ดทั้งหมดในไฟล์เดียว (ตอนส่งเปลี่ยนชื่อเป็น Assignment2_<studentID>.cpp)
├── Shaders/                      vertex / fragment shaders (ถูกคัดลอกไปข้าง .exe อัตโนมัติ)
├── Models/                       โมเดล .obj (ถูกคัดลอกไปข้าง .exe อัตโนมัติ)
├── Textures/                     รูป texture (ถูกคัดลอกไปข้าง .exe อัตโนมัติ)
├── Screenshots/                  ภาพที่แคปจากโปรแกรม
├── References/                   CREDITS.md + inspired/ (รูปแรงบันดาลใจ)
└── Submission/                   รวมไฟล์สุดท้ายก่อนส่ง
```

## Build (VS Code)
เปิดโฟลเดอร์ `Assignment2` นี้โดยตรงใน VS Code → CMake Tools เลือก preset `win-msvc-2026` → Build / Run
