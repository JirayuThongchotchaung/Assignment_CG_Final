# Assignment 2 — "FUTURE ME" (Computer Graphics)

> ไฟล์นี้สรุปเงื่อนไขของงาน ใช้ตรวจสอบก่อนส่งเพื่อไม่ให้ผิดเงื่อนไข

**Deadline:** วันจันทร์ที่ 19 ตุลาคม 2026 เวลา 23:59 น.
**ช่องทางส่ง:** Google Classroom เท่านั้น

---

## 1. โจทย์ต้นฉบับ

```
Use vertices to create a 800 x 600 image
Work in pairs
Draw image in a theme of "FUTURE ME"
Use Modern OpenGL with lighting model. You can use the external model and textures.
Submit through Google Classroom only
Submit C++ files named "Assignment2_studentID.cpp" with required shaders and the captured image
  named "Assignment2_studentID_yourPairID.jpg" or "Assignment2_studentID_yourPairID.png",
  for example, Assignment2_67050001_67050002
Write every member's name and student ID in your group in Google Classroom submission comment
Every "inspired" image must also be referenced in Google Classroom submission comment
Submit before 23.59 of Monday 19 October 2026
```

---

## 2. เงื่อนไขบังคับ (ห้ามพลาด)

| # | เงื่อนไข | ความหมาย / วิธีทำ |
|---|---|---|
| 1 | **ขนาดภาพ 800 × 600** | `glfwCreateWindow(800, 600, ...)` + `glViewport(0, 0, 800, 600)` และภาพที่ส่งต้องมีขนาด 800×600 พอดี |
| 2 | **สร้างจาก vertices** | ทุกรูปทรงต้องมาจาก vertex data ที่ส่งเข้า GPU ผ่าน VAO/VBO (/EBO) แล้ววาดด้วย `glDrawArrays` / `glDrawElements` |
| 3 | **ทำเป็นคู่** | สมาชิก 2 คน |
| 4 | **ธีม "FUTURE ME"** | ฉากต้องสื่อว่าในอนาคตเราจะเป็นอะไรหรืออยู่ที่ไหน |
| 5 | **Modern OpenGL** | OpenGL 3.3+ Core Profile และต้องเขียน vertex shader และ fragment shader (GLSL) เอง |
| 6 | **มี Lighting model** | Phong หรือ Blinn-Phong (ambient + diffuse + specular) และ vertex ต้องมี normal |
| 7 | **ส่งผ่าน Google Classroom เท่านั้น** | ห้ามส่งทางอีเมลหรือช่องทางอื่น |
| 8 | **ตั้งชื่อไฟล์ให้ถูกต้อง** | ดูหัวข้อ 4 |
| 9 | **ใส่ชื่อและรหัสสมาชิกทุกคนใน comment** | ใส่ใน submission comment ของ Classroom |
| 10 | **ใส่ reference ทุกแหล่ง** | ทุกรูปที่เป็นแรงบันดาลใจ (inspired) รวมถึงโมเดลและ texture ภายนอก ต้องใส่ลิงก์ใน comment |
| 11 | **ส่งก่อน 23:59 น. วันจันทร์ที่ 19 ต.ค. 2026** | อย่ารอส่งนาทีสุดท้าย |

### ❌ ห้ามใช้ (Legacy / Fixed-function OpenGL)
- `glBegin()` / `glEnd()` / `glVertex*()` / `glColor*()` / `glNormal*()`
- `glLight*()` / `glMaterial*()` / `glShadeModel()`
- `glMatrixMode()` / `glLoadIdentity()` / `glTranslatef()` / `glRotatef()` / `gluPerspective()` / `gluLookAt()`
- `glutSolidCube()` / `glutSolidSphere()` และ primitive สำเร็จรูปอื่นๆ ของ GLUT

### ✅ ใช้แทน
- **VAO / VBO / EBO** สำหรับเก็บ vertex data (position, normal, texcoord)
- **GLSL shaders** (`#version 330 core`)
- **GLM** สำหรับ matrix (`glm::perspective`, `glm::lookAt`, `glm::translate`, ...) แล้วส่งเข้า shader ผ่าน `uniform`
- **GLFW** + **GLAD** (หรือ GLEW) สำหรับสร้างหน้าต่างและโหลดฟังก์ชัน OpenGL

--- 

## 3. Lighting Model (หัวใจของงาน)

| องค์ประกอบ | ความหมาย | ต้องใช้ |
|---|---|---|
| **Ambient** | แสงพื้นหลังที่ส่องทั่วฉาก | `lightColor * ambientStrength` |
| **Diffuse** | ความสว่างตามมุมที่แสงตกกระทบผิว | **normal**, ตำแหน่งแสง → `max(dot(N, L), 0.0)` |
| **Specular** | จุดแสงสะท้อนบนผิวเงา | ตำแหน่งกล้อง, shininess → `pow(max(dot(V, R), 0.0), shininess)` |

- ต้องเห็นผลของทั้ง 3 ส่วนในภาพได้ชัดเจน
- ต้องแปลง normal ด้วย normal matrix: `mat3(transpose(inverse(model))) * aNormal`

**ทำเพิ่มเพื่อคะแนนพิเศษ (ไม่บังคับ):**
- ใช้แสงหลายดวง (Directional + Point + Spotlight)
- ใส่ Attenuation (แสงอ่อนลงตามระยะ)
- ใช้ Diffuse map และ Specular map
- กำหนด material แยกตามวัตถุ
- ทำ Emissive สำหรับจอหรือไฟนีออน (เข้ากับธีมอนาคต)

---

## 4. ไฟล์ที่ต้องส่ง

| ไฟล์ | รูปแบบชื่อ | ตัวอย่าง |
|---|---|---|
| โค้ด C++ | `Assignment2_studentID.cpp` | `Assignment2_67050001.cpp` |
| Shaders | ชื่อไฟล์ตามที่ใช้ในโค้ด | `Assignment2_67050001.vs`, `Assignment2_67050001.fs` |
| ภาพผลลัพธ์ | `Assignment2_studentID_yourPairID.png` หรือ `.jpg` | `Assignment2_67050001_67050002.png` |
| โมเดล / texture (ถ้าใช้) | ตามไฟล์จริง | `models/robot.obj`, `textures/metal.png` |

### ⚠️ จุดที่โจทย์ยังไม่ชัด → ต้องถามอาจารย์
- [ ] ไฟล์ `.cpp` ใช้รหัสของคนเดียว แต่ไฟล์รูปใช้รหัสทั้งคู่ **ต้องส่งทั้งสองคน หรือส่งคนเดียว** และ `.cpp` ใช้รหัสของใคร
- [ ] **ต้องแนบไฟล์โมเดลและ texture ไปด้วยหรือไม่** ถ้าอาจารย์จะรันโค้ดเอง ต้องมีไฟล์เหล่านี้ด้วย
- [ ] ชื่อไฟล์ shader มีรูปแบบบังคับหรือไม่

---

## 5. การแคปภาพ

- **แนะนำ:** บันทึกจากในโปรแกรมด้วย `glReadPixels` + `stb_image_write` จะได้ภาพ 800×600 พอดี ไม่ติดขอบหน้าต่าง
- ภาพจาก `glReadPixels` จะกลับหัว ต้องใช้ `stbi_flip_vertically_on_write(1)` ก่อนบันทึก
- ถ้าใช้ screenshot ต้อง crop ให้เหลือ 800×600 และ**ไม่มี title bar ติดมา**
- ตรวจขนาดก่อนส่ง: คลิกขวาที่ไฟล์ → Properties → Details ต้องเป็น 800 × 600

```cpp
// ตัวอย่าง: กดปุ่ม P เพื่อบันทึกภาพ
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

void saveScreenshot(const char* filename) {
    std::vector<unsigned char> pixels(800 * 600 * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, 800, 600, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    stbi_flip_vertically_on_write(1);
    stbi_write_png(filename, 800, 600, 3, pixels.data(), 800 * 3);
}
```

---

## 6. Template ข้อความ Comment ใน Google Classroom

```
Assignment 2 — FUTURE ME

สมาชิก:
1. [ชื่อ-นามสกุล] — รหัส 6705xxxx
2. [ชื่อ-นามสกุล] — รหัส 6705xxxx

แนวคิดของภาพ:
[อธิบายสั้นๆ ว่าฉากสื่อถึงอนาคตของเราอย่างไร]

References:
[Inspired images]
- [ชื่อรูป] — [ลิงก์]

[3D Models]
- [ชื่อโมเดล] by [ผู้สร้าง] — [ลิงก์] — License: [CC0 / CC-BY 4.0 / ...]

[Textures]
- [ชื่อ texture] by [ผู้สร้าง] — [ลิงก์] — License: [...]

[Code / Tutorials]
- LearnOpenGL — https://learnopengl.com
```

---

## 7. ตารางจดเครดิต (จดทุกครั้งที่โหลดไฟล์)

| ประเภท | ชื่อ | ผู้สร้าง | ลิงก์ | License |
|---|---|---|---|---|
| Model | | | | |
| Texture | | | | |
| Inspired image | | | | |

### แหล่งโหลดที่แนะนำ
- **โมเดล:** Sketchfab (ใช้ filter Downloadable), Poly Pizza, Kenney (CC0), Quaternius (CC0), NASA 3D Resources
- **Texture:** Poly Haven (CC0), ambientCG (CC0), LearnOpenGL resources
- **ฟอร์แมตที่ควรใช้:** `.obj` + `.mtl` (โหลดง่ายที่สุด) และเลือกโมเดล low-poly

---

## 8. แผนการทำงาน (7 – 19 ต.ค. 2026)

| วันที่ | งาน | เสร็จ |
|---|---|---|
| 7–8 ต.ค. | ตั้งค่าโปรเจกต์ GLFW + GLAD + GLM แล้วเปิดหน้าต่าง 800×600 ให้ได้ | [ ] |
| 8–9 ต.ค. | วาด cube พร้อม normal และทำ Phong shader ให้ทำงาน | [ ] |
| 9–10 ต.ค. | คุยธีมกับคู่ หาโมเดลและ texture **พร้อมจดเครดิต** | [ ] |
| 11–13 ต.ค. | โหลดโมเดลและ texture เข้ามา และจัดวางฉาก | [ ] |
| 14–15 ต.ค. | ปรับแสง สี และมุมกล้องให้ภาพสวย | [ ] |
| 16 ต.ค. | แคปภาพ ตั้งชื่อไฟล์ เขียน comment | [ ] |
| 17–18 ต.ค. | เผื่อเวลาแก้บั๊ก แล้ว**ส่งงาน** | [ ] |
| 19 ต.ค. 23:59 | ⛔ Deadline | |

---

## 9. ✅ Checklist ก่อนกดส่ง

### โค้ด
- [ ] ใช้ OpenGL 3.3+ Core Profile (`GLFW_OPENGL_CORE_PROFILE`)
- [ ] **ไม่มี** `glBegin` / `glEnd` / `glLight` / `glMatrixMode` / `glutSolid*`
- [ ] วาดทุกอย่างผ่าน VAO/VBO
- [ ] มี vertex shader และ fragment shader ที่เขียนเอง
- [ ] Lighting มี ambient + diffuse + specular ครบ
- [ ] ทุก vertex หรือโมเดลมี normal
- [ ] หน้าต่างขนาด 800 × 600
- [ ] โค้ดคอมไพล์และรันได้บนเครื่องอื่น (ใช้ path แบบ relative ไม่ใช่ `C:\Users\...`)

### ภาพ
- [ ] ขนาด **800 × 600** พอดี
- [ ] ไม่มีขอบหน้าต่างหรือ title bar
- [ ] เห็นแสงและเงาชัด (มี highlight จาก specular)
- [ ] สื่อธีม **FUTURE ME** ได้ชัด
- [ ] เป็นภาพที่เรนเดอร์จากโปรแกรมจริง

### ไฟล์
- [ ] `Assignment2_<studentID>.cpp`
- [ ] ไฟล์ shader ครบทุกไฟล์ที่โค้ดเรียกใช้
- [ ] `Assignment2_<ID1>_<ID2>.png` (หรือ `.jpg`)
- [ ] ไฟล์โมเดลและ texture (ถ้าอาจารย์ให้แนบ)

### Google Classroom
- [ ] ส่งผ่าน Google Classroom เท่านั้น
- [ ] Comment มี**ชื่อและรหัสของทั้ง 2 คน**
- [ ] Comment มี **reference ครบทุกแหล่ง** (inspired image, model, texture)
- [ ] กด **Turn in / ส่ง** แล้วสถานะเปลี่ยนเป็น "Turned in"
- [ ] ส่งก่อน **19 ต.ค. 2026 23:59 น.**
