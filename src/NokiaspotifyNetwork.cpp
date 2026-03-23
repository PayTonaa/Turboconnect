#include "CNokiaspotifyNetwork.h"

CNokiaspotifyNetwork::CNokiaspotifyNetwork():
    iSelectedIapId(0),
    iIsConnected(EFalse),
    iConnectionStarting(EFalse)
	{}

CNokiaspotifyNetwork::~CNokiaspotifyNetwork()
{
    iConnection.Close();
    iSocketServer.Close();
}

void CNokiaspotifyNetwork::loadSavedIapL(){
    //todo
}

void CNokiaspotifyNetwork::screenaveIapL(TUint32 aId){
    //todo
}

void CNokiaspotifyNetwork::clearSavedIap(){
    //todo
}

TBool CNokiaspotifyNetwork::hasSavedIap() const{
    //todo
}
