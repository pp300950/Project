"""
สคริปต์แปลงไฟล์วิดีโอ (mp4) เป็นไฟล์เสียง (mp3)
ใช้งาน: ติดตั้ง moviepy ก่อนด้วยคำสั่ง  pip install moviepy
"""

import os
from moviepy import VideoFileClip  # ถ้าใช้ moviepy เวอร์ชันเก่า ให้เปลี่ยนเป็น: from moviepy.editor import VideoFileClip

# ===== กำหนดพาธไฟล์ตรงนี้ =====
input_video = r"C:\Users\lenovo\OneDrive\Documents\8273e60e-a00a-4b19-a59c-54473ef05fd6.mp4"
output_folder = r"E:\music"

# ตั้งชื่อไฟล์ mp3 ตามชื่อไฟล์วิดีโอเดิม
file_name = os.path.splitext(os.path.basename(input_video))[0]
output_mp3 = os.path.join(output_folder, file_name + ".mp3")


def convert_video_to_mp3(video_path, mp3_path):
    # สร้างโฟลเดอร์ปลายทางถ้ายังไม่มี
    os.makedirs(os.path.dirname(mp3_path), exist_ok=True)

    print(f"กำลังเปิดไฟล์วิดีโอ: {video_path}")
    video = VideoFileClip(video_path)

    print(f"กำลังแปลงเป็น mp3 และบันทึกที่: {mp3_path}")
    video.audio.write_audiofile(mp3_path)

    video.close()
    print("เสร็จสิ้น! แปลงไฟล์สำเร็จแล้ว ✅")


if __name__ == "__main__":
    convert_video_to_mp3(input_video, output_mp3)