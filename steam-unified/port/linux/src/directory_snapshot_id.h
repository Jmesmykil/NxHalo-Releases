#ifndef HALO_DIRECTORY_SNAPSHOT_ID_H
#define HALO_DIRECTORY_SNAPSHOT_ID_H
#include <string.h>
/* A stable local row key, not a peer identity or authentication token.
 * Protocol family namespaces identical CE/PC endpoints. No wire change. */
static void directory_snapshot_identifier(char const *endpoint,int pc,unsigned char out[6])
{
 unsigned long long hash=14695981039346656037ULL;int i;
 hash^=(unsigned char)(pc?1:0);hash*=1099511628211ULL;
 while(*endpoint){hash^=(unsigned char)*endpoint++;hash*=1099511628211ULL;}
 for(i=0;i<6;i++)out[i]=(unsigned char)(hash>>(8*i));
 if(!(out[0]|out[1]|out[2]|out[3]|out[4]|out[5]))out[0]=1;
}
/* Resolve a previously selected row after collection/reordering. */
static int directory_selection_index(unsigned char const key[6],int version,
 unsigned char const *keys,int const *versions,int count,int stride)
{
 int i;
 if(!keys || !versions || count<=0 || stride<6 ||
    !(key[0]|key[1]|key[2]|key[3]|key[4]|key[5]))return -1;
 for(i=0;i<count;i++)
  if(versions[i]==version && !memcmp(keys+i*stride,key,6))return i;
 return -1;
}
#endif
