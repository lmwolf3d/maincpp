#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include <io.h>
#include <windows.h>
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <direct.h>
#include <stdint.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <shellapi.h>
#include <stdlib.h>

using namespace std;

#define USE_HIRES

#ifdef USE_HIRES
#define SPRITESCALEFACTOR 1
#else
#define SPRITESCALEFACTOR 2
#endif

int ChunksInFile;
int PMSpriteStart;
int PMSoundStart;
bool PMSoundInfoPagePadded;
uint32_t *PMPageData;
size_t PMPageDataSize;
uint8_t **PMPages;
std::string OutPutFilePath;
const char *VSWAPPATH;
static byte *vbuf = NULL;
unsigned vbufPitch = 0;


typedef uint16_t word;

template <typename T>
  std::string NumberToString ( T Number )
  {
     std::ostringstream ss;
     ss << Number;
     return ss.str();
  }

int ChunkNum = 0;
bool SetChunkZero = true;


short ReadShort(std::vector<uint8_t> ChunkData, int position) {
			short val = (short)(ChunkData[position] | ChunkData[position + 1] << 8);
			return val;
}

static inline uint32_t PM_GetPageSize(int page)
{
    return (uint32_t) (PMPages[page + 1] - PMPages[page]);
}

static inline uint8_t *PM_GetPage(int page)
{
    return PMPages[page];
}

static inline uint8_t *PM_GetEnd()
{
    return PMPages[ChunksInFile];
}

static inline byte *PM_GetTexture(int wallpic)
{
    return PM_GetPage(wallpic);
}

static inline uint16_t *PM_GetSprite(int shapenum)
{
    return (uint16_t *) (void *) PM_GetPage(PMSpriteStart + shapenum);
}

static inline byte *PM_GetSound(int soundpagenum)
{
    return PM_GetPage(PMSoundStart + soundpagenum);
}

typedef struct
{
    word leftpix,rightpix;
    word dataofs[64];
} t_compshape;

static inline word READWORD(byte *&ptr)
{
    word val = ptr[0] | ptr[1] << 8;
    ptr += 2;
    return val;
}
static byte CHARTOBYTE(byte B)
{
	for(int i = 0;i < 256; i++) {
		if(((byte)i) == B) {
			return ((byte)i);
		}
	}
	return ((byte)0);
}

void PM_Startup()
{
    FILE *file = fopen(VSWAPPATH, "rb");
	if(!file) {
		return;
	}
    ChunksInFile = 0;
    fread(&ChunksInFile, sizeof(word), 1, file);
    PMSpriteStart = 0;
    fread(&PMSpriteStart, sizeof(word), 1, file);
    PMSoundStart = 0;
    fread(&PMSoundStart, sizeof(word), 1, file);

    uint32_t* pageOffsets = (uint32_t *) malloc((ChunksInFile + 1) * sizeof(int32_t));
    fread(pageOffsets, sizeof(uint32_t), ChunksInFile, file);
    word *pageLengths = (word *) malloc(ChunksInFile * sizeof(word));
    fread(pageLengths, sizeof(word), ChunksInFile, file);

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    long pageDataSize = fileSize - pageOffsets[0];
    if(pageDataSize > (size_t) -1) {
		return;
		//        Quit("The page file \"%s\" is too large!", fname);
	}

    pageOffsets[ChunksInFile] = fileSize;

    uint32_t dataStart = pageOffsets[0];
    int i;

    // Check that all pageOffsets are valid
    for(i = 0; i < ChunksInFile; i++)
    {
        if(!pageOffsets[i]) continue;   // sparse page
        if(pageOffsets[i] < dataStart || pageOffsets[i] >= (size_t) fileSize)
		{
			return;
			// Quit("Illegal page offset for page %i: %u (filesize: %u)", i, pageOffsets[i], fileSize);
		}

    }

    // Calculate total amount of padding needed for sprites and sound info page
    int alignPadding = 0;
    for(i = PMSpriteStart; i < PMSoundStart; i++)
    {
        if(!pageOffsets[i]) continue;   // sparse page
        uint32_t offs = pageOffsets[i] - dataStart + alignPadding;
        if(offs & 1)
            alignPadding++;
    }

    if((pageOffsets[ChunksInFile - 1] - dataStart + alignPadding) & 1)
        alignPadding++;

    PMPageDataSize = (size_t) pageDataSize + alignPadding;
    PMPageData = (uint32_t *) malloc(PMPageDataSize);
    PMPages = (uint8_t **) malloc((ChunksInFile + 1) * sizeof(uint8_t *));

    uint8_t *ptr = (uint8_t *) PMPageData;
     for(i = 0; i < ChunksInFile; i++)
    {
        if(i >= PMSpriteStart && i < PMSoundStart || i == ChunksInFile - 1)
        {
            size_t offs = ptr - (uint8_t *) PMPageData;
            if(offs & 1)
            {
                *ptr++ = 0;
                if(i == ChunksInFile - 1) PMSoundInfoPagePadded = true;
            }
        }

        PMPages[i] = ptr;

        if(!pageOffsets[i])
            continue;

        uint32_t size;
        if(!pageOffsets[i + 1]) size = pageLengths[i];
        else size = pageOffsets[i + 1] - pageOffsets[i];
        fseek(file, pageOffsets[i], SEEK_SET);
        fread(ptr, 1, size, file);
        ptr += size;
    }

    PMPages[ChunksInFile] = ptr;
    free(pageLengths);
    free(pageOffsets);
    fclose(file);
	byte *addr;
	int viewheight, viewwidth;
	viewheight = 320 * 2;
	viewwidth = 640 * 2;
	std::ofstream ff(OutPutFilePath.c_str(), ios::out | ios::binary);
    for (i = 0; i < (PMSoundStart - PMSpriteStart); i++)
    {
    addr = (byte *) PM_GetPage(i);
    if (addr)
    {
	cout << "[" + NumberToString(i) + "]";
    ff << "[Sprite " + NumberToString(i) + "]";
    int xcenter;
    int shapenum;
    unsigned height;
    xcenter = viewwidth/2;
    shapenum = i;
    height = viewheight+1;
    t_compshape   *shape;
    unsigned scale,pixheight;
    unsigned starty,endy;
    word *cmdptr;
    byte *cline;
    byte *line;
    int actx,i,upperedge;
    short newstart;
    int scrstarty,screndy,lpix,rpix,pixcnt,ycnt;
    unsigned j;
	int viewheight, viewwidth;
	viewheight = 320 * 2;
	viewwidth = 640 * 2;
	vbufPitch = 640;
    byte col;
    byte *vmem;
    shape = (t_compshape *) PM_GetSprite(shapenum);
    scale=height>>1;
    pixheight=scale*SPRITESCALEFACTOR;
    actx=xcenter-scale;
    upperedge=viewheight/2-scale;
    cmdptr=shape->dataofs;
    for(i=shape->leftpix,pixcnt=i*pixheight,rpix=(pixcnt>>6)+actx;i<=shape->rightpix;i++,cmdptr++)
    {
        lpix=rpix;
        if(lpix>=viewwidth) break;
        pixcnt+=pixheight;
        rpix=(pixcnt>>6)+actx;
        if(lpix!=rpix && rpix>0)
        {
            if(lpix<0) lpix=0;
            if(rpix>viewwidth) rpix=viewwidth,i=shape->rightpix+1;
            cline = (byte *)shape + *cmdptr;
            while(lpix<rpix)
            {
                line=cline;
                while((endy = READWORD(line)) != 0)
                {
                    endy >>= 1;
                    newstart = READWORD(line);
                    starty = READWORD(line) >> 1;
                    j=starty;
                    ycnt=j*pixheight;
                    screndy=(ycnt>>6)+upperedge;
					if(screndy<0) {
						vmem=vbuf+lpix;
					} else {
						vmem=vbuf+screndy*vbufPitch+lpix;
					}

                    for(;j<endy;j++)
                    {
                        scrstarty=screndy;
                        ycnt+=pixheight;
                        screndy=(ycnt>>6)+upperedge;
                        if(scrstarty!=screndy && screndy>0)
                        {
                            col=((byte *)shape)[newstart+j];
							//Write Bytes To OutPut File
                            ff << CHARTOBYTE(i); //i=X
							ff << CHARTOBYTE(j); //j=Y
							ff << CHARTOBYTE(col); //col=Color (256 color)
                            if(scrstarty<0) scrstarty=0;
                            if(screndy>viewheight) screndy=viewheight,j=endy;
                        }
                    }
                }
                lpix++;
            }
        }
    }
    }
	}
}

int main() {
		VSWAPPATH = "C:\\VSWAP.WL6";
        OutPutFilePath = "C:\\AllSprites.Dat";
        PM_Startup();
		cout << endl << "All Sprites Data Written To : " + OutPutFilePath;
		char temp;
		cin.get(temp);
}