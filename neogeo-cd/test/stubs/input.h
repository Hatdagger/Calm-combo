extern unsigned char stub_in[8];
#define P1_EDGE   ((unsigned long)&stub_in[0])
#define P1_REPEAT ((unsigned long)&stub_in[1])
#define P2_EDGE   ((unsigned long)&stub_in[2])
#define P2_REPEAT ((unsigned long)&stub_in[3])
#define PS_EDGE   ((unsigned long)&stub_in[4])
#define JOY_UP 0x01
#define JOY_DOWN 0x02
#define JOY_LEFT 0x04
#define JOY_RIGHT 0x08
#define JOY_A 0x10
#define JOY_B 0x20
#define JOY_C 0x40
#define JOY_D 0x80
#define P1_START 0x01
#define P1_SELECT 0x02
#define P2_START 0x04
#define P2_SELECT 0x08
