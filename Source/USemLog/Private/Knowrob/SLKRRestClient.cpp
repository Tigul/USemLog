#include "Knowrob/SLKRRestClient.h"

#include "CoreMinimal.h"
#include "IWebSocket.h"
#include "Containers/Queue.h"
#include <string>


FSLKRRestClient::FSLKRRestClient()
{
}

FSLKRRestClient::~FSLKRRestClient()
{
}

void FSLKRRestClient::Init(const FString& InHost, int32 InPort, const FString& InProtocol) {

    //URL = TEXT("172.31.115.208:62226/knowrob/api/v1.0/query");
    URL = InHost + TEXT(":") + FString::FromInt(InPort) + TEXT("/knowrob/api/v1.0/query");
    
    IsConnected();

}
//\"query\": \"true\",
bool FSLKRRestClient::IsConnected() {
    //SendRequest(TEXT("{ \"query\": \"A=2.\", \"maxSolutionCount\" : 1}"));
    return true;
}

void FSLKRRestClient::SendRequest(FString RequestContent) {
   
    FHttpModule& httpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> pRequest = httpModule.CreateRequest();

    pRequest->SetVerb(TEXT("POST"));
    pRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    FString Query = FString::Printf(TEXT("{ \"query\": \"%s\", \"maxSolutionCount\" : 1}"), *RequestContent);
    pRequest->SetContentAsString(*Query);

    pRequest->SetURL(URL);

    pRequest->OnProcessRequestComplete().BindLambda(
        // Here, we "capture" the 'this' pointer (the "&"), so our lambda can call this
        // class's methods in the callback.
        [&](
            FHttpRequestPtr pRequest,
            FHttpResponsePtr pResponse,
            bool connectedSuccessfully) mutable {

                if (connectedSuccessfully) {

                    // We should have a JSON response - attempt to process it.
                   // ProcessSpaceTrackResponse(pResponse->GetContentAsString());
                    UE_LOG(LogTemp, Warning, TEXT("Success"));
                    ProcessKnowrobResponse(pResponse->GetContentAsString());
                }
                else {
                    switch (pRequest->GetStatus()) {
                    case EHttpRequestStatus::Failed_ConnectionError:
                        UE_LOG(LogTemp, Error, TEXT("Connection failed."));
                    default:
                        UE_LOG(LogTemp, Error, TEXT("Request failed."));
                    }
                }
        });

    pRequest->ProcessRequest();
}

void FSLKRRestClient::ProcessKnowrobResponse(const FString& ResponseContent) {
    // Validate http called us back on the Game Thread...
    check(IsInGameThread());

    TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ResponseContent);
    TArray<TSharedPtr<FJsonValue>> OutArray;
    TSharedPtr<FJsonObject> OutObject;
    FJsonSerializer::Deserialize(JsonReader, OutObject);

    
    //*OutObject->GetStringField(TEXT("query")
    TArray<TSharedPtr<FJsonValue>> ResponesArray =  OutObject->GetArrayField(TEXT("response"));
    UE_LOG(LogTemp, Warning, TEXT("Length: %d"), ResponesArray.Num());
    for (TSharedPtr<FJsonValue> elem : ResponesArray) {
        TSharedPtr<FJsonObject> obj = elem->AsObject();
        UE_LOG(LogTemp, Warning, TEXT("answere: %s"), *obj->GetStringField(TEXT("A")));
        //UE_LOG(LogTemp, Warning, TEXT("query response: %s"), elem->AsObject());
       
    }
}