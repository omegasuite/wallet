#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef unsigned char     uint8;
typedef unsigned long    uint32;

static uint8 alphabet_map[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static uint8 reverse_map[] =
{
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62, 255, 255, 255, 63,
      52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255, 255, 255, 255, 255, 255,
      255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
      15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 255, 255, 255, 255, 255,
      255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
      41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 255, 255, 255, 255, 255
};

uint32 base64_encode(const char * text, uint32 text_len, char* encode)
{
     uint32 i, j;
     for (i = 0, j = 0; i + 3 <= text_len; i += 3)
     {
         encode[j++] = alphabet_map[text[i] >> 2];                             //ȡ����һ���ַ���ǰ6λ���ҳ���Ӧ�Ľ���ַ�
         encode[j++] = alphabet_map[((text[i] << 4) & 0x30) | (text[i + 1] >> 4)];     //����һ���ַ��ĺ�2λ��ڶ����ַ���ǰ4λ������ϲ��ҵ���Ӧ�Ľ���ַ�
         encode[j++] = alphabet_map[((text[i + 1] << 2) & 0x3c) | (text[i + 2] >> 6)];   //���ڶ����ַ��ĺ�4λ��������ַ���ǰ2λ��ϲ��ҳ���Ӧ�Ľ���ַ�
         encode[j++] = alphabet_map[text[i + 2] & 0x3f];                         //ȡ���������ַ��ĺ�6λ���ҳ�����ַ�
     }

     if (i < text_len)
     {
         uint32 tail = text_len - i;
         if (tail == 1)
         {
             encode[j++] = alphabet_map[text[i] >> 2];
             encode[j++] = alphabet_map[(text[i] << 4) & 0x30];
             encode[j++] = '=';
             encode[j++] = '=';
         }
         else //tail==2
         {
             encode[j++] = alphabet_map[text[i] >> 2];
             encode[j++] = alphabet_map[((text[i] << 4) & 0x30) | (text[i + 1] >> 4)];
             encode[j++] = alphabet_map[(text[i + 1] << 2) & 0x3c];
             encode[j++] = '=';
         }
     }
     return j;
}

uint32 base64_decode(const char * code, uint32 code_len, char * plain)
{
     assert((code_len & 0x03) == 0);  //��������������ش�������ֹ����ִ�С�4�ı�����

     uint32 i, j = 0;
     uint8 quad[4];
     for (i = 0; i < code_len; i += 4)
     {
         for (uint32 k = 0; k < 4; k++)
         {
             quad[k] = reverse_map[code[i + k]];//���飬ÿ���ĸ��ֱ�����ת��Ϊbase64���ڵ�ʮ������
         }

         assert(quad[0] < 64 && quad[1] < 64);

         plain[j++] = (quad[0] << 2) | (quad[1] >> 4); //ȡ����һ���ַ���Ӧbase64���ʮ��������ǰ6λ��ڶ����ַ���Ӧbase64���ʮ��������ǰ2λ�������

         if (quad[2] >= 64)
             break;
         else if (quad[3] >= 64)
         {
             plain[j++] = (quad[1] << 4) | (quad[2] >> 2); //ȡ���ڶ����ַ���Ӧbase64���ʮ�������ĺ�4λ��������ַ���Ӧbase64���ʮ��������ǰ4λ�������
             break;
         }
         else
         {
             plain[j++] = (quad[1] << 4) | (quad[2] >> 2);
             plain[j++] = (quad[2] << 6) | quad[3];//ȡ���������ַ���Ӧbase64���ʮ�������ĺ�2λ���4���ַ��������
         }
     }
     return j;
}


/*int main(void)
88 {
89     char input[256];
90     while (true){
91     printf("Please input string: ");
92     scanf("%s", input);
93     uint8 *text = (uint8 *)input;
94     uint32 text_len = (uint32)strlen((char *)text);
95     uint8 buffer[1024], buffer2[4096];
96     uint32 size = base64_encode(text, text_len, buffer2);
97     buffer2[size] = 0;
98     printf("%s\n", buffer2);
99
100     size = base64_decode(buffer2, size, buffer);
101     buffer[size] = 0;
102     printf("%s\n", buffer);
103
104     }
105     return 0;
106 }*/

/*
int main(void)
109 {
	110     char input[256];
	111     while (true) {
		112     printf("Please input what you want to decode: ");
		113     scanf("%s", input);
		114     uint8 * text = (uint8*)input;
		115     uint32 text_len = (uint32)strlen((char*)text);
		116     uint8 buffer[1024], buffer2[4096];
		117
			118     uint32 size = base64_decode(text, text_len, buffer);
		119     buffer[size] = 0;
		120     printf("Decoded content: %s\n", buffer);
		121     size = base64_encode(buffer, size, buffer2);
		122     buffer2[size] = 0;
		123     printf("Confirmation of the original content: %s\n", buffer2);
		124
	}
	125     return 0;
	126 }
*/