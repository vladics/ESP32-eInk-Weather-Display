//#########################################################################################
void ClearSky(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) {
    scale = Large;
    y = y - 6; // Shift up large sun
  }
  else {
    y = y - 5; // Shift down small sun icon
  }
  if (IconName.endsWith("n")) addmoon(x, y + 3, scale, IconSize);
  scale = scale * 1.6;
  addsun(x, y, scale, IconSize);
}
//#########################################################################################
void FewClouds(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 1;
  if (IconSize == LargeIcon) {
    scale = Large;
    linesize = 3;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
}
//#########################################################################################
void ScatteredClouds(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 3, offset = 10;
  if (IconSize == LargeIcon) {
    scale = Large;
  }
  else
  {
    linesize = 1;
    offset   = 5;
  }
  if (IconName.endsWith("n")) addmoon(x, y + offset + (IconSize ? -8 : 0), scale, IconSize);
  addcloud(x + offset, y - offset * 1.2, scale / 1.5, linesize);
  addcloud(x, y + offset, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale, IconSize);
}
//#########################################################################################
void BrokenClouds(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 3, offset  = 12;
  if (IconSize == LargeIcon) {
    scale = Large;
  }
  else
  {
    linesize = 1;
    offset   = 6;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x - offset, y - offset, scale / 1.5, linesize);
  addcloud(x + offset, y - offset * 1.2, scale / 1.5, linesize);
  addcloud(x, y, scale, linesize);
}
//#########################################################################################
void Rain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void ChanceRain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x - (IconSize ? 8 : 0), y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void Tstorms(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addtstorm(x, y, scale);
}
//#########################################################################################
void Snow(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addsnow(x, y, scale, IconSize);
}
//#########################################################################################
void Mist(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
    y = y + 5;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y - 5, scale, linesize);
  addfog(x, y - 2, scale, linesize, IconSize);
}
//#########################################################################################
void Haze(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x, y - 2, scale * 1.4, IconSize);
  addfog(x, y + 3 - (IconSize ? 12 : 0), scale * 1.4, linesize, IconSize);
}
//#########################################################################################
void ExpectRain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void CloudCover(int x, int y, int CCover) {
  addcloud(x - 9, y - 3, Small * 0.6, 2); // Cloud top left
  addcloud(x + 3, y - 3, Small * 0.6, 2); // Cloud top right
  addcloud(x, y,         Small * 0.6, 2); // Main cloud
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 15, y - 5, String(CCover) + "%", LEFT);
}
//#########################################################################################
void Nodata(int x, int y, bool IconSize, String IconName) {
  if (IconSize == LargeIcon) u8g2Fonts.setFont(u8g2_font_helvB24_tf); else u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 3, y - 8, "?", CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
}