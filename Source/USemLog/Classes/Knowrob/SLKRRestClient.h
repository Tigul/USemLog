#pragma once

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"

#include "CoreMinimal.h"
#include "IWebSocket.h"
#include "Containers/Queue.h"
#include "SLKRResponseStruct.h"
#include <string>


class USEMLOG_API FSLKRRestClient
{
public:

	FSLKRRestClient();
	
	~FSLKRRestClient();


	void Init(const FString& InHost, int32 InPort, const FString& InProtocol);

	bool IsConnected();

	void SendRequest(FString RequestContent);

private:
	FString URL;

	void ProcessKnowrobResponse(const FString& ResponseContent);

	
};