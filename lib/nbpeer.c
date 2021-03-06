/*
* Copyright (C) 2014 Ed Trettevik <eat@nodebrain.org>
*
* NodeBrain is free software; you can modify and/or redistribute it under the
* terms of either the MIT License (Expat) or the following NodeBrain License.
*
* Permission to use and redistribute with or without fee, in source and binary
* forms, with or without modification, is granted free of charge to any person
* obtaining a copy of this software and included documentation, provided that
* the above copyright notice, this permission notice, and the following
* disclaimer are retained with source files and reproduced in documention
* included with source and binary distributions. 
*
* Unless required by applicable law or agreed to in writing, this software is
* distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.
*
*=============================================================================
* Program: NodeBrain
*
* File:    nbpeer.c
*
* Title:   NodeBrain Peer Communication Handler
*
* Purpose:
*
*   This file provides functions supporting non-blocking socket
*   communication between peer nodes.  It is intended for situations
*   where data may flow in both directions as asynchronous messages.
*   This API has no clue what the data is, only that it is organized
*   as variable length records.  Higher level API's can apply
*   structure to the records.
*
* Synopsis:
*
*   nbPeer *nbPeerConstruct(nbCELL context,char *uri,nbCELL tlsContext,void *handle,
*     int (*producer)(nbCELL context,nbPeer *peer,void *handle),
*     int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len));
*     void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code));
*   nbPeer *nbPeerModify(nbCELL context,nbPeer *peer,void *handle,
*     int (*producer)(nbCELL context,nbPeer *peer,void *handle),
*     int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len));
*     void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code));
*   nbPeer *nbPeerListen(nbCELL context,nbPeer *peer);
*   int nbPeerConnect(nbCELL context,nbPeer *peer,void *handle,
*     int (*producer)(nbCELL context,nbPeer *peer,void *handle,int code),
*     int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len),
*     void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code));
*   int nbPeerSend(nbCELL context,nbPeer *peer,void *data,int len);
*   int nbPeerShutdown(nbCELL context,nbPeer *peer,code);
*   int nbPeerDestroy(nbCELL context,nbPeer *peer);
*
*
* NOTE:
*
*   The nbPeerConsumer and nbPeerProducer functions have been modified to manage
*   the nbListener wait conditions. We assume that nbPeerReader and nbPeerWriter
*   are the only listener exits we want to use once the first exit has been taken.
*
* Description:
*
*   This API provides a simple layer on top of the NodeBrain Medulla API and
*   NodeBrain TLS API to exchange data blocks up to 64KB using non-blocking
*   IO via encrypted and authenticated connection.  
*
*   The Medulla API manages when sockets are ready for read or write.  This
*   file provides handlers for the Medulla API.
*
*   The TLS API manages encryption and a layer of authentication and
*   authorization.  The TLS layer is configured using a NodeBrain context,
*   so the NodeBrain rule author has the ability to configure it.
*
*   Two user provided handler functions are required.
*
*     producer - Called when
*                  a) connection has been accepted by listening peer
*                  b) connection has been accomplished after nbPeerConnect call
*                  c) the buffer has been written 
*
*                Code:
*                  -1 - sorry man, things didn't work out with the connection
*                   0 - everything is fine
*                  
*                Returns:
*                  -1 - shutdown the connection
*                   0 - data provided (or not - buffer is checked)
*                  
*     consumer - Called for every record received from the peer
*
*                Returns:
*                  -1 - shutdown the connection
*                   0 - data handled
*
*   The consumer is passed the data as originally sent to nbPeerSend.
*   There should be no assumption that a 2 byte length field is available
*   ahead of the data.  Protocols requiring a length prefix should include
*   their own prefix.  This may seem redundant, but it enables modifications
*   here without impacting applications.
*
*   A void pointer called "handle" is provided to these functions for use
*   as a session handle. The user's notion of a protocol state can be
*   managed as an attribute in the session handle, or by updating the
*   producer and consumer handlers.
*
*   We are focused here on supporting asynchronous non-blocking data flow.
*   However, the functions may be enhancement in the future to support
*   blocking by setting the producer and/or consumer to null values and
*   including an nbPeerRecv function.  We will look at the requirements
*   of an enhanced "peer" module at that time.  For now we are addressing
*   the requirements of the "message" module.
*
*==============================================================================
* Change History:
*
*    Date    Name/Change
* ---------- -----------------------------------------------------------------
* 2010/01/07 Ed Trettevik (original prototype)
* 2010/02/25 eat 0.7.9  Cleaned up -Wall warning messages
* 2010/02/26 eat 0.7.9  Cleaned up -Wall warning messages (gcc 4.1.2)
* 2010/06/06 eat 0.8.2  Added client parameter to nbTlsLoadContext parameter
* 2011-02-08 eat 0.8.5  Refined non-blocking SSL handshake
* 2012-10-13 eat 0.8.12 Replaced malloc/free with nbAlloc/nbFree
* 2012-12-27 eat 0.8.13 Checker updates
* 2013-01-01 eat 0.8.13 Checker updates
* 2013-01-11 eat 0.8.13 Checker updates
* 2014-02-01 eat 0.8.16 Made TLS optional
*==============================================================================
*/
#include "../config.h"
#ifdef HAVE_OPENSSL

#include <nb/nb.h>

int peerTrace;          // debugging trace flag for peer routines
/*********************************************************************
* Medulla Event Handlers
*********************************************************************/

/*
*  Write data to peer
*/ 
static void nbPeerWriter(nbCELL context,int sd,void *handle){
  nbPeer *peer=(nbPeer *)handle;
  int len;
  size_t size;
  int code;

  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerWriter: called for sd=%d",sd);
  size=peer->wloc-peer->wbuf;
  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerWriter: called for sd=%d size=%d",sd,size);
  if(size){
    len=nbTlsWrite(peer->tls,(char *)peer->wbuf,size);
    if(len<0){
      if(peer->tls->error==NB_TLS_ERROR_WANT_WRITE) return; // will try again later
      nbLogMsg(context,0,'E',"nbPeerWriter: nbTlsWrite failed - %s",strerror(errno));
      peer->flags|=NB_PEER_FLAG_WRITE_ERROR;
      }
    if(len<size){
      size-=len;
      memcpy(peer->wbuf,peer->wloc+len,size);
      peer->wloc=peer->wbuf+size;
      } 
    else peer->wloc=peer->wbuf;
    }
  if(peer->producer){
    if((code=(*peer->producer)(context,peer,peer->handle))){  // call producer for more data
      nbPeerShutdown(context,peer,code);
      }
    if(peer->wloc==peer->wbuf){  // if we don't have more data to write, stop waiting to write
      if(peerTrace) nbLogMsg(context,0,'T',"nbPeerWriter: removing WRITE_WAIT on SD=%d because we have no more data at the moment",sd);
      nbListenerRemoveWrite(context,sd);
      peer->flags&=0xff-NB_PEER_FLAG_WRITE_WAIT;
      }
    }
  else{
    if(peerTrace) nbLogMsg(context,0,'T',"nbPeerWriter: removing WRITE_WAIT on SD=%d because we have no producer",sd);
    nbListenerRemoveWrite(context,sd);
    peer->flags&=0xff-NB_PEER_FLAG_WRITE_WAIT;
    }
  }

/*
*  Read data from peer
*/
static void nbPeerReader(nbCELL context,int sd,void *handle){
  nbPeer *peer=(nbPeer *)handle;
  int len;
  size_t size;
  nbTLS *tls=peer->tls;
  unsigned char *bufcur,*dataend;
  int code;
  
  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerReader: called for sd=%d",sd);
  if(!peer->consumer){  // this should not happen
    nbLogMsg(context,0,'T',"nbPeerReader: data available but no consumer - removing wait");
    nbListenerRemove(context,sd);
    peer->flags&=0xff-NB_PEER_FLAG_READ_WAIT;
    nbPeerShutdown(context,peer,-1);
    return;
    }
  size=NB_PEER_BUFLEN-(peer->rloc-peer->rbuf);
  len=nbTlsRead(tls,(char *)peer->rloc,size);
  if(len<=0){
    if(len<0 && tls->error==NB_TLS_ERROR_WANT_READ) return; // will try again later
    if(len==0) nbLogMsg(context,0,'I',"nbPeerReader: Peer %d %s has shutdown connection",sd,tls->uriMap[tls->uriIndex].uri);
    else nbLogMsg(context,0,'E',"nbPeerReader: Peer %d %s unable to read - %s",sd,tls->uriMap[tls->uriIndex].uri,strerror(errno));
    peer->flags|=NB_PEER_FLAG_WRITE_ERROR;
    if(len==0) nbPeerShutdown(context,peer,0);
    else nbPeerShutdown(context,peer,-1);
    return;
    } 
  // have to deblock the messages here
  dataend=peer->rloc+len;
  bufcur=peer->rbuf;

  while(bufcur<dataend && peer->consumer){
    if(dataend-peer->rbuf<2){
      if(peerTrace) nbLogMsg(context,0,'T',"nbPeerReader: message length is split - have to read again");
      peer->rloc+=len;
      if(bufcur!=peer->rbuf){
        memcpy(peer->rbuf,bufcur,peer->rloc-bufcur);
        peer->rloc-=bufcur-peer->rbuf;
        if(peer->rloc<peer->rbuf){
           nbLogMsg(context,0,'L',"nbPeerReader: Peer %d %s fatal logic error - rloc %p < rbuf %p",sd,tls->uriMap[tls->uriIndex].uri,peer->rloc,peer->rbuf);
           exit(1);
           }
        }
      return;
      }
    len=(*bufcur<<8)|*(bufcur+1);
    if(bufcur+len>dataend){
      if(peerTrace) nbLogMsg(context,0,'T',"nbPeerReader: didn't get full message - have to read again");
      peer->rloc=dataend;
      if(bufcur!=peer->rbuf){
        memcpy(peer->rbuf,bufcur,dataend-bufcur);
        peer->rloc-=bufcur-peer->rbuf;
        if(peer->rloc<peer->rbuf){
           nbLogMsg(context,0,'L',"nbPeerReader: Peer %d %s fatal logic error - rloc %p < rbuf %p",sd,tls->uriMap[tls->uriIndex].uri,peer->rloc,peer->rbuf);
           exit(1);
           }
        }
      return;
      }
    // call the consumer
    if(peerTrace) nbLogMsg(context,0,'T',"nbPeerReader: calling the consumer exit - peer->handle=%p",peer->handle);
    if((code=(*peer->consumer)(context,peer,peer->handle,bufcur+2,len-2))){
      nbLogMsg(context,0,'T',"nbPeerReader: Peer %d %s shutting down by consumer request",sd,tls->uriMap[tls->uriIndex].uri);
      nbListenerRemove(context,sd); // shutdown should do this for us
      peer->flags&=0xff-NB_PEER_FLAG_READ_WAIT;
      nbPeerShutdown(context,peer,code);
      return;
      }
    bufcur+=len;
    //nbLogMsg(context,0,'T',"nbPeerReader: new bufcur=%p",bufcur);
    }
  if(peer->consumer){
    if(bufcur!=dataend){
      nbLogMsg(context,0,'T',"nbPeerReader: Peer %d %s fatal error - didn't finish at end of data - terminating",sd,tls->uriMap[tls->uriIndex].uri);
      exit(1);
      }
    peer->rloc=peer->rbuf;  // next read will be at start of buffer
    }
  else{ // the consumer has been cancelled in the middle of reading
    nbLogMsg(context,0,'L',"nbPeerReader: Peer %d %s fatal defect - consumer bailed in middle of sending buffer - terminating",sd,tls->uriMap[tls->uriIndex].uri);
    exit(1);
    }
  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerReader: returning");
  }


/*
*  Call producer after TLS handshake is complete 
*
*    It doesn't seem necessary to call the producer here.
*    Can't we just let the nbPeerWriter routine call it
*    and respond?  Need to walk through this.
*/
static void nbPeerConnecter(nbCELL context,int sd,void *handle){
  nbPeer *peer=(nbPeer *)handle;
  int code;

  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerConnecter: Peer %d flags=%x",sd,peer->flags);
  if(peer->producer){
    if((code=(*peer->producer)(context,peer,peer->handle))){
      nbPeerShutdown(context,peer,code);
      return;
      }
    if(peer->wloc>peer->wbuf) nbPeerWriter(context,sd,handle);
    if(peer->producer){
      nbListenerAddWrite(context,sd,peer,nbPeerWriter);
      peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
      }
    }
  else nbLogMsg(context,0,'T',"nbPeerConnecter: Peer %d flags=%x has no producer",sd,peer->flags);
  if(peer->consumer){
    nbListenerAdd(context,sd,peer,nbPeerReader);
    peer->flags|=NB_PEER_FLAG_READ_WAIT;
    }
  else nbLogMsg(context,0,'T',"nbPeerConnecter: Peer %d flags=%x has no consumer",sd,peer->flags);
  nbLogMsg(context,0,'T',"nbPeerConnecter: Peer %d flags=%x",sd,peer->flags);
  }

/*
*  Handle TLS handshake when OpenSSL wants read or write during connect 
*/
static void nbPeerConnectHandshaker(nbCELL context,int sd,void *handle){
  nbPeer *peer=(nbPeer *)handle;
  int rc;

  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerConnectHandshaker: called");
  if(peer->flags&NB_PEER_FLAG_WRITE_WAIT){
    nbListenerRemoveWrite(context,sd);
    peer->flags&=0xff-NB_PEER_FLAG_WRITE_WAIT;
    }
  if(peer->flags&NB_PEER_FLAG_READ_WAIT){
    nbListenerRemove(context,sd);
    peer->flags&=0xff-NB_PEER_FLAG_READ_WAIT;
    }
  if((rc=nbTlsConnectHandshake(peer->tls))==0){
    nbLogMsg(context,0,'I',"Peer connection established with %s",nbTlsGetUri(peer->tls));
    nbListenerAddWrite(context,sd,peer,nbPeerConnecter);
    peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
    }
  else if(rc==SSL_ERROR_WANT_WRITE){
    if(peerTrace) nbLogMsg(context,0,'T',"nbPeerConnectHandshaker: SSL_ERROR_WANT_WRITE");
    nbListenerAddWrite(context,sd,peer,nbPeerConnectHandshaker);
    peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
    }
  else if(rc==SSL_ERROR_WANT_READ){
    if(peerTrace) nbLogMsg(context,0,'T',"nbPeerConnectHandshaker: SSL_ERROR_WANT_READ");
    nbListenerAdd(context,sd,peer,nbPeerConnectHandshaker);
    peer->flags|=NB_PEER_FLAG_READ_WAIT;
    }
  else{
    nbLogMsg(context,0,'T',"nbPeerConnectHandshaker: handshake failed rc=%d - shutting down",rc);
    nbPeerShutdown(context,peer,-1);
    }
  }

/*
*  Handle TLS handshake when OpenSSL wants read or write during accept
*/
static void nbPeerAcceptHandshaker(nbCELL context,int sd,void *handle){
  nbPeer *peer=(nbPeer *)handle;
  int rc;

  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerAcceptHandshaker: called");
  if(peer->flags&NB_PEER_FLAG_WRITE_WAIT){
    nbListenerRemoveWrite(context,sd);
    peer->flags&=0xff-NB_PEER_FLAG_WRITE_WAIT;
    }
  if(peer->flags&NB_PEER_FLAG_READ_WAIT){
    nbListenerRemove(context,sd);
    peer->flags&=0xff-NB_PEER_FLAG_READ_WAIT;
    }
  rc=nbTlsAcceptHandshake(peer->tls);
  if(rc==1){
    nbLogMsg(context,0,'T',"Peer connection with %s established",nbTlsGetUri(peer->tls));
    nbListenerAddWrite(context,sd,peer,nbPeerConnecter);
    peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
    }
  else if(rc==-1){
    if(peer->tls->error==NB_TLS_ERROR_WANT_WRITE){
      if(peerTrace) nbLogMsg(context,0,'T',"nbPeerAcceptHandshaker: SSL_ERROR_WANT_WRITE");
      nbListenerAddWrite(context,sd,peer,nbPeerAcceptHandshaker);
      peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
      }
    else if(peer->tls->error==NB_TLS_ERROR_WANT_READ){
      if(peerTrace) nbLogMsg(context,0,'T',"nbPeerAcceptHandshaker: SSL_ERROR_WANT_READ");
      nbListenerAdd(context,sd,peer,nbPeerAcceptHandshaker);
      peer->flags|=NB_PEER_FLAG_READ_WAIT;
      }
    else{
      nbLogMsg(context,0,'T',"nbPeerAcceptHandshaker: handshake failed");
      nbPeerShutdown(context,peer,-1);
      }
    }
  else{
    nbLogMsg(context,0,'T',"nbPeerAcceptHandshaker: handshake failed rc=%d - shutting down",rc);
    nbPeerShutdown(context,peer,-1);
    }
  }

/*
*  Accept a connection and call the producer
*
*    It is typical for the producer of a listening peer to
*    provide a new producer and handle when called.  The
*    initial producer may handle the exchange required to
*    construct or locate a new handle.
*/
static void nbPeerAccepter(nbCELL context,int sd,void *handle){
  nbPeer *peer,*lpeer=(nbPeer *)handle;
  nbTLS *tls;

  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerAccepter: called for sd=%d",sd);
  tls=nbTlsAccept(lpeer->tls);
  if(!tls){
    nbLogMsg(context,0,'T',"nbPeerAccepter: nbTlsAccept failed");
    return;
    }
  nbLogMsg(context,0,'T',"nbPeerAccepter: nbTlsAccept succeeded");
  peer=(nbPeer *)nbAlloc(sizeof(nbPeer));
  memcpy(peer,lpeer,sizeof(nbPeer));
  peer->tls=tls;
  if(!peer->wbuf) peer->wbuf=(unsigned char *)nbAlloc(NB_PEER_BUFLEN);
  peer->wloc=peer->wbuf;
  if(!peer->rbuf) peer->rbuf=(unsigned char *)nbAlloc(NB_PEER_BUFLEN);
  peer->rloc=peer->rbuf;
  // 2011-02-05 eat - this has been reworked for non-blocking IO
  if(tls->option==NB_TLS_OPTION_TCP || tls->error==NB_TLS_ERROR_UNKNOWN){
    nbLogMsg(context,0,'T',"nbPeerAccepter: setting up nbPeerConnecter");
    nbListenerAddWrite(context,tls->socket,peer,nbPeerConnecter);
    peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
    }
  else if(tls->error==NB_TLS_ERROR_WANT_WRITE){
    nbListenerAddWrite(context,tls->socket,peer,nbPeerAcceptHandshaker);
    peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
    }
  else if(tls->error==NB_TLS_ERROR_WANT_READ){
    nbListenerAdd(context,tls->socket,peer,nbPeerAcceptHandshaker);
    peer->flags|=NB_PEER_FLAG_READ_WAIT;
    }
  nbLogMsg(context,0,'T',"nbPeerAccepter: returning");
  }


/*********************************************************************
* API - Functions below are API functions.  Those above are internal.
*********************************************************************/

/*
*  Create a peer structure for listening or connecting.
*
*    Buffers are not allocated until a connection is established.
*/
nbPeer *nbPeerConstruct(nbCELL context,int client,char *uriName,char *uri,nbCELL tlsContext,void *handle,
  int (*producer)(nbCELL context,nbPeer *peer,void *handle),
  int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len),
  void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code)){

  nbPeer *peer;
  nbTLSX  *tlsx;

  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerConstruct: called uri=%s",uri);
  // allocate a peer structure
  peer=(nbPeer *)nbAlloc(sizeof(nbPeer));
  memset(peer,0,sizeof(nbPeer));
  peer->handle=handle;
  peer->producer=producer;
  peer->consumer=consumer;
  peer->shutdown=shutdown;
  uri=nbTermOptionString(tlsContext,uriName,uri);
  nbLogMsg(context,0,'T',"nbPeerConstruct: configured uri=%s",uri);
  tlsx=nbTlsLoadContext(context,tlsContext,peer,client);
  peer->tls=nbTlsCreate(tlsx,uri);
  if(peer->tls) nbLogMsg(context,0,'T',"nbPeerCreate: called uri=%s",uri);
  else nbLogMsg(context,0,'T',"nbPeerConstruct: unable to create tls for uri=%s",uri);
  return(peer);
  }

/*
*  Start listening as a peer
*
*    The peer structure is cloned when a connection is accepted
*    and the producer is invoked.  The producer may then call other
*    API functions to change the handle, producer, or consumer, and
*    may also call nbPeerSend.  The peer structure created by nbPeerListen
*    does not have allocated buffers.
*    
*/
int nbPeerListen(nbCELL context,nbPeer *peer){
  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerListen: called uri=%s",peer->tls->uriMap[0].uri);
  if(nbTlsListen(peer->tls)<0){
    nbLogMsg(context,0,'E',"Unable to listen - %s",peer->tls->uriMap[0].uri);
    return(-1);
    }
  if(fcntl(peer->tls->socket,F_SETFL,fcntl(peer->tls->socket,F_GETFL)|O_NONBLOCK)){ // 2012-12-27 eat 0.8.13 - CID 751523
    nbLogMsg(context,0,'E',"Unable to listen - %s - unable to set non-blocking - %s",peer->tls->uriMap[0].uri,strerror(errno));
    return(-1);
    }
  nbListenerAdd(context,peer->tls->socket,peer,nbPeerAccepter);
  peer->flags|=NB_PEER_FLAG_READ_WAIT;
  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerListen: returning - handing off to nbPeerAccepter");
  return(0);
  }

/*
*  Establish a connection with a peer
*
*    Buffers are allocated under the assumption that the connection will
*    be successful, now or in the future.  The buffers are freed when
*    a peer is shutdown or destroyed.
*
*  Returns:
*
*     -1 - connection failed
*      0 - connecting
*      1 - connected successfully
*/
int nbPeerConnect(nbCELL context,nbPeer *peer,void *handle,
  int (*producer)(nbCELL context,nbPeer *peer,void *handle),
  int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len),
  void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code)){

  int rc;

  if(!peer || !peer->tls){
    nbLogMsg(context,0,'E',"nbPeerConnect: called with bad pointer");
    return(-1);
    }
  //nbLogMsg(context,0,'I',"Attempting peer connection with %s",peer->tls->uriMap[0].uri);
  nbLogMsg(context,0,'I',"Attempting peer connection with %s",nbTlsGetUri(peer->tls));
  if(!peer->wbuf) peer->wbuf=(unsigned char *)nbAlloc(NB_PEER_BUFLEN);
  peer->wloc=peer->wbuf;
  if(!peer->rbuf) peer->rbuf=(unsigned char *)nbAlloc(NB_PEER_BUFLEN);
  peer->rloc=peer->rbuf;
  peer->handle=handle;
  peer->producer=producer;
  peer->consumer=consumer;
  peer->shutdown=shutdown;
  rc=nbTlsConnectNonBlocking(peer->tls);
  if(rc<0){
    nbLogMsg(context,0,'W',"nbPeerConnect: Unable to connect - %s",strerror(errno));
    nbPeerShutdown(context,peer,-1);
    return(-1);
    }
  switch(rc){
    case 0:
      nbLogMsg(context,0,'I',"Peer connection established with %s",nbTlsGetUri(peer->tls));
      nbListenerAddWrite(context,peer->tls->socket,peer,nbPeerConnecter);
      peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
      return(1);
    case 1:
      nbListenerAddWrite(context,peer->tls->socket,peer,nbPeerConnectHandshaker);
      peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
      if(peerTrace) nbLogMsg(context,0,'I',"nbPeerConnect: returning - good luck waiting for a connection");
      return(0); 
    case 2:
      nbListenerAdd(context,peer->tls->socket,peer,nbPeerConnectHandshaker);
      peer->flags|=NB_PEER_FLAG_READ_WAIT;
      if(peerTrace) nbLogMsg(context,0,'T',"nbPeerConnect: returning - good luck waiting for a connection");
      return(0); 
    }
  nbLogMsg(context,0,'L',"nbPeerConnect: unexpected return code %d from nbTlsConnectNonBlocking");
  return(rc);
  }

/*
*  Buffer data up for peer and send as soon as possible
*
*  Returns:
*
*    -1 - error encountered - need to shutdown
*     0 - success
*     1 - buffer is full - will call producer when ready for retry
*/
int nbPeerSend(nbCELL context,nbPeer *peer,void *data,uint16_t size){
  uint16_t mysize;

  if(peerTrace) nbLogMsg(context,0,'I',"Sending %d bytes to %s",size,nbTlsGetUri(peer->tls));
  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerSend: called with peer=%p SD=%d size=%d flags=%x",peer,peer->tls->socket,size,peer->flags);
  if(peer->flags&NB_PEER_FLAG_WRITE_ERROR){
    nbLogMsg(context,0,'T',"nbPeerSend: error flag is set");
    return(-1);
    }
  if(size>NB_PEER_BUFLEN-2){
    nbLogMsg(context,0,'T',"nbPeerSend: size is too big for buffer");
    return(-1);
    }
  if(peer->wloc+size+2>peer->wbuf+NB_PEER_BUFLEN){
    nbLogMsg(context,0,'T',"nbPeerSend: buffer is full");
    return(1);
    }
  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerSend: made it past bail out conditions");
  // put message in buffer
  memcpy(peer->wloc+2,data,size);
  mysize=size+2;               // 2013-01-01 eat - VID 5268-0.8.13-1 FP but changed data type and moved here from init
  *peer->wloc=(mysize>>8)%256; // 2013-01-01 eat - VID 5376-0.8.13-1 FP but changed data type of mysize from int to uint16_t
                               // 2013-01-11 eat - VID 6583-0.8.13-2 FP but still trying to help checker by adding the %256
  peer->wloc++;
  *peer->wloc=mysize%256;      // 2013-01-01 eat - VID 4614,4849-0.8.13-1 FP but changed from &0xff to %256 and data type
  peer->wloc++;
  peer->wloc+=size;
  if(!(peer->flags&NB_PEER_FLAG_WRITE_WAIT)){
    if(peerTrace) nbLogMsg(context,0,'T',"nbPeerSend: nbListenerAddWrite SD=%d flags=%x",peer->tls->socket,peer->flags);
    nbListenerAddWrite(context,peer->tls->socket,peer,nbPeerWriter);
    peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
    }
  return(0);
  }

void nbPeerModify(nbCELL context,nbPeer *peer,void *handle,
  int (*producer)(nbCELL context,nbPeer *peer,void *handle),
  int (*consumer)(nbCELL context,nbPeer *peer,void *handle,void *data,int len),
  void (*shutdown)(nbCELL context,nbPeer *peer,void *handle,int code)){

  peer->handle=handle;

  peer->producer=producer;
  if(!producer){
    if(peer->flags&NB_PEER_FLAG_WRITE_WAIT){
      nbListenerRemoveWrite(context,peer->tls->socket);
      peer->flags&=0xff-NB_PEER_FLAG_WRITE_WAIT;
      }
    }
  else{
    if(!(peer->flags&NB_PEER_FLAG_WRITE_WAIT)){
      nbListenerAddWrite(context,peer->tls->socket,peer,nbPeerWriter);
      peer->flags|=NB_PEER_FLAG_WRITE_WAIT;
      }
    }

  peer->consumer=consumer; 
  if(!consumer){
    if(peer->flags&NB_PEER_FLAG_READ_WAIT){
      nbListenerRemove(context,peer->tls->socket);
      peer->flags&=0xff-NB_PEER_FLAG_READ_WAIT;
      }
    }
  else{
    if(!(peer->flags&NB_PEER_FLAG_READ_WAIT)){
      nbListenerAdd(context,peer->tls->socket,peer,nbPeerReader);
      peer->flags|=NB_PEER_FLAG_READ_WAIT;
      }
    }

  peer->shutdown=shutdown;
  }

/*
*  Shutdown peer connection
*
*    If it would be helpful for the producer and consumer to receive
*    a reason for the shutdown, there are a couple options.  The caller
*    could set a code in the peer structure before calling nbPeerShutdown,
*    or an additional int parameter can be added to nbPeerShutdown.
*/
int nbPeerShutdown(nbCELL context,nbPeer *peer,int code){
  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerShutdown: %s code=%d",peer->tls->uriMap[peer->tls->uriIndex].uri,code);
  if(peer->shutdown) (*peer->shutdown)(context,peer,peer->handle,code);
  if(peer->flags&NB_PEER_FLAG_WRITE_WAIT){
    nbListenerRemoveWrite(context,peer->tls->socket);
    peer->flags&=0xff-NB_PEER_FLAG_WRITE_WAIT;
    }
  if(peer->flags&NB_PEER_FLAG_READ_WAIT){
    nbListenerRemove(context,peer->tls->socket);
    peer->flags&=0xff-NB_PEER_FLAG_READ_WAIT;
    }
  if(peer->tls) nbTlsClose(peer->tls); // do this after the removes because it clears the socket
  peer->flags&=0xff-NB_PEER_FLAG_WRITE_ERROR;
  if(peer->wbuf) nbFree(peer->wbuf,NB_PEER_BUFLEN);
  peer->wbuf=NULL;
  if(peer->rbuf) nbFree(peer->rbuf,NB_PEER_BUFLEN);
  peer->rbuf=NULL;
  peer->producer=NULL;
  peer->consumer=NULL;
  peer->shutdown=NULL;
  return(0);
  }

int nbPeerDestroy(nbCELL context,nbPeer *peer){
  if(peerTrace) nbLogMsg(context,0,'T',"nbPeerDestroy: called");
  if(peer->tls) nbLogMsg(context,0,'T',"nbPeerDestroy: uri=%s",peer->tls->uriMap[0].uri);
  if(peer->tls) nbTlsFree(peer->tls);
  if(peer->wbuf) nbFree(peer->wbuf,NB_PEER_BUFLEN);
  if(peer->rbuf) nbFree(peer->rbuf,NB_PEER_BUFLEN);
  nbFree(peer,sizeof(nbPeer));
  return(0);
  }

#endif
