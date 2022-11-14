#include <ESP8266WiFi.h>
#include <Ultrasonic.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>

#define WIFI_SSID
#define WIFI_PASSWORD 
#define API_KEY 
#define USER_EMAIL 
#define USER_PASSWORD 
#define FIREBASE_PROJECT_ID 
#define TRIG_PIN_1 4
#define ECHO_PIN_1 5
#define TRIG_PIN_2 12
#define ECHO_PIN_2 14


// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long dataMillis = 0;
int persons;

Ultrasonic ultrasonic1(TRIG_PIN_1, ECHO_PIN_1);
Ultrasonic ultrasonic2(TRIG_PIN_2, ECHO_PIN_2);

int ultrasonic1Current;
bool present1 = false;

int ultrasonic2Current;
bool present2 = false;

int personCount;

void connectToWiFi()
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void initializeFirebase()
{
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

#if defined(ESP8266)
  // In ESP8266 required for BearSSL rx/tx buffer for large data handle, increase Rx size as needed.
  fbdo.setBSSLBufferSize(1024 /* Rx buffer size in bytes from 512 - 16384 */, 4096 /* Tx buffer size in bytes from 512 - 16384 */);
#endif

  // Limit the size of response payload to be collected in FirebaseData
  fbdo.setResponseSize(2048);

  Firebase.begin(&config, &auth);

  Firebase.reconnectWiFi(true);
}

int getPersons() {
  String documentPath = "halls/classroom";

  // If the document path contains space e.g. "a b c/d e f"
  // It should encode the space as %20 then the path will be "a%20b%20c/d%20e%20f"

  Serial.print("Getting initial persons... ");

  if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), "")) {

      // Create a FirebaseJson object and set content with received payload
      FirebaseJson payload;
      payload.setJsonData(fbdo.payload().c_str());

      // Get the data from FirebaseJson object 
      FirebaseJsonData jsonData;
      payload.get(jsonData, "fields/persons/integerValue", true);

      return jsonData.intValue;
  }
  else {
      Serial.println(fbdo.errorReason());
      return 0;
  }
}

void setup()
{
  Serial.begin(9600);
  Serial.println();

  connectToWiFi();
  initializeFirebase();
  persons = getPersons();
  Serial.println("Persons: ");
  Serial.println(persons);
}

void incrementFirebaseField(String value)
{
  dataMillis = millis();

  Serial.print("Commit a document (field value increment)... ");
  Serial.print(value);
  Serial.println();

  // The dyamic array of write object fb_esp_firestore_document_write_t.
  std::vector<struct fb_esp_firestore_document_write_t> writes;

  // A write object that will be written to the document.
  struct fb_esp_firestore_document_write_t transform_write;

  // Set the write object write operation type.
  transform_write.type = fb_esp_firestore_document_write_type_transform;

  // Set the document path of document to write (transform)
  transform_write.document_transform.transform_document_path = "halls/classroom";

  // Set a transformation of a field of the document.
  struct fb_esp_firestore_document_write_field_transforms_t field_transforms;

  // Set field path to write.
  field_transforms.fieldPath = "persons";

  // Set the transformation type.
  field_transforms.transform_type = fb_esp_firestore_transform_type_increment;

  // For the usage of FirebaseJson, see examples/FirebaseJson/BasicUsage/Create.ino
  FirebaseJson values;

  values.set("integerValue", value);

  // Set the values of field
  field_transforms.transform_content = values.raw();

  // Add a field transformation object to a write object.
  transform_write.document_transform.field_transforms.push_back(field_transforms);

  // Add a write object to a write array.
  writes.push_back(transform_write);

  if (Firebase.Firestore.commitDocumentAsync(&fbdo, FIREBASE_PROJECT_ID, "" /* databaseId can be (default) or empty */, writes /* dynamic array of fb_esp_firestore_document_write_t */, "" /* transaction */))
    Serial.println("ok");
  else
    Serial.println(fbdo.errorReason());
}

void loop()
{
  // Firebase.ready() should be called repeatedly to handle authentication tasks.
  if (Firebase.ready() && (millis() - dataMillis > 20 || dataMillis == 0))
  {
    ultrasonic1Current = ultrasonic1.read();
    if (ultrasonic1Current != 0 && ultrasonic1Current != 23)
    {

      if (ultrasonic1Current < 80)
      {
        present1 = true;
      }
      else
      {
        if (present1 == true)
        {
          persons++;
          incrementFirebaseField("1");
        }
        present1 = false;
      }
    }
    ultrasonic2Current = ultrasonic2.read();
    if (ultrasonic2Current != 0 && ultrasonic2Current != 23)
    {
      if (ultrasonic2Current < 80)
      {
        present2 = true;
      }
      else
      {
        if (present2 == true && persons != 0)
        {
          persons--;
          incrementFirebaseField("-1");
        }
        present2 = false;
      }
    }
  }
  // low delay to count people who move through it fast
  delay(500);
}
