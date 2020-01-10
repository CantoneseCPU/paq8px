#include "WordEmbeddingDictionary.hpp"
#include "../Hash.hpp"

int WordEmbeddingDictionary::findEntry(const short prefix, const uint8_t suffix) {
  int i = hash(prefix, suffix) % hashSize;
  int offset = (i > 0) ? hashSize - i : 1;
  while( true ) {
    if( table[i] < 0 ) //free slot?
      return -i - 1;
    else if( entries[table[i]].prefix == prefix && entries[table[i]].suffix == suffix ) //is it the entry we want?
      return table[i];
    i -= offset;
    if( i < 0 )
      i += hashSize;
  }
}

void WordEmbeddingDictionary::addEntry(const short prefix, const uint8_t suffix, const int offset) {
  if( prefix == -1 || prefix >= index || index > 0x7FFF || offset >= 0 )
    return;
  entries[index].prefix = prefix;
  entries[index].suffix = suffix;
  table[-offset - 1] = index;
  index += (index < 0x8000);
}

WordEmbeddingDictionary::WordEmbeddingDictionary() : entries(0x8000), table(hashSize), index(0) { reset(); }

WordEmbeddingDictionary::~WordEmbeddingDictionary() {
#ifndef NVERBOSE
  if (requests > 0)
      printf("\nHits: %d, Requests: %d, %.2f%%\n", hits, requests, (hits * 100.0) / requests);
#endif
}

void WordEmbeddingDictionary::reset() {
  for( index = 0; index < hashSize; table[index] = -1, index++ );
  for( index = 0; index < 256; index++ ) {
    table[-findEntry(-1, index) - 1] = index;
    entries[index].prefix = -1;
    entries[index].suffix = index;
  }
#ifndef NVERBOSE
  requests = hits = 0;
#endif
}

bool WordEmbeddingDictionary::addWord(const Word *w, const uint32_t embedding) {
  bool res = false;
  int parent = -1, code = 0, len = w->length();
  if( len == 0 )
    return res;
  for( int i = 0; i < len; i++ ) {
    int idx = findEntry(parent, code = (*w)[i]);
    if( idx < 0 ) {
      addEntry(parent, code, idx);
      parent = index - 1;
      res = true;
    } else
      parent = idx;
  }
  assert(parent >= 0);
  if( !res )
    res = !entries[parent].termination;
  entries[parent].termination = true;
  entries[parent].embedding = embedding;
  return res;
}

void WordEmbeddingDictionary::getWordEmbedding(Word *w) {
  int parent = -1;
#ifndef NVERBOSE
  requests++;
#endif
  w->embedding = -1;
  for( uint32_t i = 0; i < w->length(); i++ ) {
    if((parent = findEntry(parent, (*w)[i])) < 0 )
      return;
  }
  if( !entries[parent].termination )
    return;
  w->embedding = entries[parent].embedding;
#ifndef NVERBOSE
  hits++;
#endif
}

void WordEmbeddingDictionary::loadFromFile(const char *filename) {
  FileDisk f;
#ifndef NVERBOSE
  if (toScreen) printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
    printf("Loading word embeddings...");
#endif
  OpenFromMyFolder::anotherFile(&f, filename);
  Word w;
  int byte = 0, embedding = 0, total = 0;
  do {
    if( f.blockRead((uint8_t *) (&embedding), WORD_EMBEDDING_SIZE) != WORD_EMBEDDING_SIZE )
      break;
    w.reset();
    while((byte = f.getchar()) >= 0 && byte != 0x0A )
      w += byte;
    if( addWord(&w, embedding))
      total++;
  } while( byte >= 0 );
#ifndef NVERBOSE
  printf(" done [%s, %d words]\n", filename, total);
#endif
  f.close();
}
