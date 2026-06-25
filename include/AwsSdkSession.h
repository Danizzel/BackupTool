/**
 *quasi der "Ein und Aus Schalter" für die SDK (aws verlangt am Anfang Aws::InitAPI() und am Ende Aws::ShutdownAPI())
 */

#pragma once
#include <aws/core/Aws.h>

class AwsSdkSession {
public:
    //Konstruktor zum Start der SDK
    AwsSdkSession() {
        Aws::InitAPI(options);
    }

    //Destruktor zum Beenden der SKD
    ~AwsSdkSession() {
        Aws::ShutdownAPI(options);
    }

private:
    Aws::SDKOptions options;

};