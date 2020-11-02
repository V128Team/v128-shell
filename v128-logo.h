#ifndef V128_LOGO_H
#define V128_LOGO_H

struct v128_logo_t {
  unsigned int 	 width;
  unsigned int 	 height;
  unsigned int 	 bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */ 
  unsigned char	 pixel_data[800 * 592 * 4 + 1];
};

extern const struct v128_logo_t v128_logo;

#endif // V128_LOGO_H
