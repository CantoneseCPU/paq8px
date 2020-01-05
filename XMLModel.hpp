#ifndef PAQ8PX_XMLMODEL_HPP
#define PAQ8PX_XMLMODEL_HPP

//////////////////////////// XMLModel //////////////////////////

#define CacheSize 32

#if (CacheSize & (CacheSize - 1)) || (CacheSize < 8)
#error cache size must be a power of 2 bigger than 4
#endif

struct XMLAttribute {
    uint32_t name, value, length;
};

struct XMLContent {
    uint32_t data, length, type;
};

struct XMLTag {
    uint32_t name, length;
    int Level;
    bool endTag, empty;
    XMLContent content;
    struct XMLAttributes {
        XMLAttribute items[4];
        uint32_t Index;
    } attributes;
};

struct XMLTagCache {
    XMLTag tags[CacheSize];
    uint32_t Index;
};

enum ContentFlags {
    Text = 0x001,
    Number = 0x002,
    Date = 0x004,
    Time = 0x008,
    URL = 0x010,
    Link = 0x020,
    Coordinates = 0x040,
    Temperature = 0x080,
    ISBN = 0x100,
};

enum XMLState {
    None = 0,
    ReadTagName = 1,
    ReadTag = 2,
    ReadAttributeName = 3,
    ReadAttributeValue = 4,
    ReadContent = 5,
    ReadCDATA = 6,
    ReadComment = 7,
};

class XMLModel {
private:
    static constexpr int nCM = 4;

public:
    static constexpr int MIXERINPUTS = nCM * (ContextMap::MIXERINPUTS); //20
    static constexpr int MIXERCONTEXTS = 0;
    static constexpr int MIXERCONTEXTSETS = 0;

private:
    const Shared *const shared;
    ContextMap cm;
    XMLTagCache cache {};
    uint32_t stateBH[8] {};
    XMLState state = None, pState = None;
    uint32_t whiteSpaceRun = 0, pWSRun = 0, indentTab = 0, indentStep = 2, lineEnding = 2;

    void DetectContent(XMLContent *Content) {
      INJECT_SHARED_c4
      INJECT_SHARED_c8
      INJECT_SHARED_buf
      if((c4 & 0xF0F0F0F0) == 0x30303030 ) { //may be 4 digits (dddd)
        int i = 0, j = 0;
        while((i < 4) && ((j = (c4 >> (8 * i)) & 0xFF) >= 0x30 && j <= 0x39))
          i++;
        if( i == 4 /*????dddd*/ &&
            (((c8 & 0xFDF0F0FD) == 0x2D30302D && buf(9) >= 0x30 && buf(9) <= 0x39 /*d-dd-dddd or d.dd.dddd*/) ||
             ((c8 & 0xF0FDF0FD) == 0x302D302D) /*d-d-dddd or d.d.dddd*/))
          (*Content).type |= ContentFlags::Date;
      } else if(((c8 & 0xF0F0FDF0) == 0x30302D30 /*dd.d???? or dd-d????*/ || (c8 & 0xF0F0F0FD) == 0x3030302D) &&
                buf(9) >= 0x30 && buf(9) <= 0x39 /*dddd-???? or dddd.????*/) {
        int i = 2, j = 0;
        while((i < 4) && ((j = (c8 >> (8 * i)) & 0xFF) >= 0x30 && j <= 0x39))
          i++;

        if( i == 4 && (c4 & 0xF0FDF0F0) == 0x302D3030 ) //dd??d.dd or dd??d-dd
          (*Content).type |= ContentFlags::Date;
      }

      if((c4 & 0xF0FFF0F0) == 0x303A3030 && buf(5) >= 0x30 && buf(5) <= 0x39 &&
         ((buf(6) < 0x30 || buf(6) > 0x39) /*?dd:dd*/ ||
          ((c8 & 0xF0F0FF00) == 0x30303A00 && (buf(9) < 0x30 || buf(9) > 0x39) /*?dd:dd:dd*/)))
        (*Content).type |= ContentFlags::Time;

      if((*Content).length >= 8 && (c8 & 0x80808080) == 0 && (c4 & 0x80808080) == 0 ) //8 ~ascii
        (*Content).type |= ContentFlags::Text;

      if((c8 & 0xF0F0FF) == 0x3030C2 &&
         (c4 & 0xFFF0F0FF) == 0xB0303027 ) { //dd {utf8 C2B0: degree sign} dd {apostrophe}
        int i = 2;
        while((i < 7) && buf(i) >= 0x30 && buf(i) <= 0x39 )
          i += (i & 1U) * 2 + 1;

        if( i == 10 )
          (*Content).type |= ContentFlags::Coordinates;
      }

      if((c4 & 0xFFFFFA) == 0xC2B042 && (c4 & 0xff) != 0x47 &&
         (((c4 >> 24) >= 0x30 && (c4 >> 24) <= 0x39) || ((c4 >> 24) == 0x20 && (buf(5) >= 0x30 && buf(5) <= 0x39))))
        (*Content).type |= ContentFlags::Temperature;

      INJECT_SHARED_c1
      if( c1 >= 0x30 && c1 <= 0x39 )
        (*Content).type |= ContentFlags::Number;

      if( c4 == 0x4953424E && (c8 & 0xff) == 0x20 ) // " ISBN"
        (*Content).type |= ContentFlags::ISBN;
    }

public:
    XMLModel(const Shared *const sh, const uint64_t size) : shared(sh), cm(sh, size, nCM) {}

    void update() {
      INJECT_SHARED_c1
      INJECT_SHARED_c4
      INJECT_SHARED_c8
      XMLTag *pTag = &cache.tags[(cache.Index - 1) & (CacheSize - 1)], *Tag = &cache.tags[cache.Index &
                                                                                          (CacheSize - 1)];
      XMLAttribute *Attribute = &((*Tag).attributes.items[(*Tag).attributes.Index & 3]);
      XMLContent *Content = &(*Tag).content;
      pState = state;
      if((c1 == TAB || c1 == SPACE) && (c1 == (uint8_t) (c4 >> 8) || !whiteSpaceRun)) {
        whiteSpaceRun++;
        indentTab = (c1 == TAB);
      } else {
        if((state == None || (state == ReadContent && (*Content).length <= lineEnding + whiteSpaceRun)) &&
           whiteSpaceRun > 1 + indentTab && whiteSpaceRun != pWSRun ) {
          indentStep = abs((int) (whiteSpaceRun - pWSRun));
          pWSRun = whiteSpaceRun;
        }
        whiteSpaceRun = 0;
      }
      if( c1 == NEW_LINE )
        lineEnding = 1 + ((uint8_t) (c4 >> 8) == CARRIAGE_RETURN);

      switch( state ) {
        case None: {
          if( c1 == 0x3C ) {
            state = ReadTagName;
            memset(Tag, 0, sizeof(XMLTag));
            (*Tag).Level = ((*pTag).endTag || (*pTag).empty) ? (*pTag).Level : (*pTag).Level + 1;
          }
          if((*Tag).Level > 1 )
            DetectContent(Content);

          cm.set(hash(pState, state, ((*pTag).Level + 1) * indentStep - whiteSpaceRun));
          break;
        }
        case ReadTagName: {
          if((*Tag).length > 0 && (c1 == TAB || c1 == NEW_LINE || c1 == CARRIAGE_RETURN || c1 == SPACE))
            state = ReadTag;
          else if((c1 == 0x3A || (c1 >= 'A' && c1 <= 'Z') || c1 == 0x5F || (c1 >= 'a' && c1 <= 'z')) ||
                  ((*Tag).length > 0 && (c1 == 0x2D || c1 == 0x2E || (c1 >= '0' && c1 <= '9')))) {
            (*Tag).length++;
            (*Tag).name = (*Tag).name * 263 * 32 + (c1 & 0xDF);
          } else if( c1 == 0x3E ) {
            if((*Tag).endTag ) {
              state = None;
              cache.Index++;
            } else
              state = ReadContent;
          } else if( c1 != 0x21 && c1 != 0x2D && c1 != 0x2F && c1 != 0x5B ) {
            state = None;
            cache.Index++;
          } else if((*Tag).length == 0 ) {
            if( c1 == 0x2F ) {
              (*Tag).endTag = true;
              (*Tag).Level = max(0, (*Tag).Level - 1);
            } else if( c4 == 0x3C212D2D ) {
              state = ReadComment;
              (*Tag).Level = max(0, (*Tag).Level - 1);
            }
          }

          if((*Tag).length == 1 && (c4 & 0xFFFF00) == 0x3C2100 ) {
            memset(Tag, 0, sizeof(XMLTag));
            state = None;
          } else if((*Tag).length == 5 && c8 == 0x215B4344 && c4 == 0x4154415B ) {
            state = ReadCDATA;
            (*Tag).Level = max(0, (*Tag).Level - 1);
          }

          int i = 1;
          do {
            pTag = &cache.tags[(cache.Index - i) & (CacheSize - 1)];
            i += 1 + ((*pTag).endTag && cache.tags[(cache.Index - i - 1) & (CacheSize - 1)].name == (*pTag).name);
          } while( i < CacheSize && ((*pTag).endTag || (*pTag).empty));

          cm.set(hash(pState, state, (*Tag).name, (*Tag).Level, (*pTag).name, (*pTag).Level != (*Tag).Level));
          break;
        }
        case ReadTag: {
          if( c1 == 0x2F )
            (*Tag).empty = true;
          else if( c1 == 0x3E ) {
            if((*Tag).empty ) {
              state = None;
              cache.Index++;
            } else
              state = ReadContent;
          } else if( c1 != TAB && c1 != NEW_LINE && c1 != CARRIAGE_RETURN && c1 != SPACE ) {
            state = ReadAttributeName;
            (*Attribute).name = c1 & 0xDF;
          }
          cm.set(hash(pState, state, (*Tag).name, c1, (*Tag).attributes.Index));
          break;
        }
        case ReadAttributeName: {
          if((c4 & 0xFFF0) == 0x3D20 && (c1 == 0x22 || c1 == 0x27)) {
            state = ReadAttributeValue;
            if((c8 & 0xDFDF) == 0x4852 && (c4 & 0xDFDF0000) == 0x45460000 )
              (*Content).type |= Link;
          } else if( c1 != 0x22 && c1 != 0x27 && c1 != 0x3D )
            (*Attribute).name = (*Attribute).name * 263 * 32 + (c1 & 0xDF);

          cm.set(hash(pState, state, (*Attribute).name, (*Tag).attributes.Index, (*Tag).name, (*Content).type));
          break;
        }
        case ReadAttributeValue: {
          if( c1 == 0x22 || c1 == 0x27 ) {
            (*Tag).attributes.Index++;
            state = ReadTag;
          } else {
            (*Attribute).value = (*Attribute).value * 263 * 32 + (c1 & 0xDF);
            (*Attribute).length++;
            if((c8 & 0xDFDFDFDF) == 0x48545450 && ((c4 >> 8) == 0x3A2F2F || c4 == 0x733A2F2F))
              (*Content).type |= URL;
          }
          cm.set(hash(pState, state, (*Attribute).name, (*Content).type));
          break;
        }
        case ReadContent: {
          if( c1 == 0x3C ) {
            state = ReadTagName;
            cache.Index++;
            memset(&cache.tags[cache.Index & (CacheSize - 1)], 0, sizeof(XMLTag));
            cache.tags[cache.Index & (CacheSize - 1)].Level = (*Tag).Level + 1;
          } else {
            (*Content).length++;
            (*Content).data = (*Content).data * 997 * 16 + (c1 & 0xDF);
            DetectContent(Content);
          }
          cm.set(hash(pState, state, (*Tag).name, c4 & 0xC0FF));
          break;
        }
        case ReadCDATA: {
          if((c4 & 0xFFFFFF) == 0x5D5D3E ) {
            state = None;
            cache.Index++;
          }
          cm.set(hash(pState, state));
          break;
        }
        case ReadComment: {
          if((c4 & 0xFFFFFF) == 0x2D2D3E ) {
            state = None;
            cache.Index++;
          }
          cm.set(hash(pState, state));
          break;
        }
      }

      stateBH[pState] = (stateBH[pState] << 8) | c1;
      pTag = &cache.tags[(cache.Index - 1) & (CacheSize - 1)];
      uint64_t i = 64;
      cm.set(hash(++i, state, (*Tag).Level, pState * 2 + (*Tag).endTag, (*Tag).name));
      cm.set(hash(++i, (*pTag).name, state * 2 + (*pTag).endTag, (*pTag).content.type, (*Tag).content.type));
      cm.set(hash(++i, state * 2 + (*Tag).endTag, (*Tag).name, (*Tag).content.type, c4 & 0xE0FF));
    }

    void mix(Mixer &m) {
      INJECT_SHARED_bpos
      if( bpos == 0 )
        update();
      cm.mix(m);
    }
};

#undef CacheSize
#endif //PAQ8PX_XMLMODEL_HPP
