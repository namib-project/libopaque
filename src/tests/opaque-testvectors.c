/*
    @copyright 2018-2020, opaque@ctrlc.hu
    This file is part of libopaque

    libopaque is free software: you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    libopaque is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libopaque. If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <assert.h>
#include "../opaque.h"
#include "../common.h"

typedef struct {
  uint8_t blind[crypto_core_ristretto255_SCALARBYTES];
  uint16_t pwdU_len;
  uint8_t pwdU[];
} Opaque_RegisterUserSec;

int main(void) {
  // test vector 1
  // create credential workflow

  uint8_t pwdU[]="CorrectHorseBatteryStaple";
  size_t pwdU_len = sizeof(pwdU) - 1;
  uint8_t ctx[OPAQUE_REGISTER_USER_SEC_LEN+pwdU_len];
  uint8_t regreq[crypto_core_ristretto255_BYTES];

  // create registration request
  if(0!=opaque_CreateRegistrationRequest(pwdU, pwdU_len, ctx, regreq)) return 1;
  // metadata is R blinding factor
  // "blind_registration": "c62937d17dc9aa213c9038f84fe8c5bf3d953356db01c4d48acb7cae48e6a504"
  const uint8_t blind_registration[] = {0xc6, 0x29, 0x37, 0xd1, 0x7d, 0xc9, 0xaa, 0x21,
                              0x3c, 0x90, 0x38, 0xf8, 0x4f, 0xe8, 0xc5, 0xbf,
                              0x3d, 0x95, 0x33, 0x56, 0xdb, 0x01, 0xc4, 0xd4,
                              0x8a, 0xcb, 0x7c, 0xae, 0x48, 0xe6, 0xa5, 0x04};
  // assert match with test vector
  if(memcmp(((Opaque_RegisterUserSec*) &ctx)->blind,blind_registration, sizeof(blind_registration))!=0) {
    fprintf(stderr, "failed to reproduce reg req blind factor\n");
    dump(blind_registration, sizeof(blind_registration),                      "blind_registration");
    dump(((Opaque_RegisterUserSec*) &ctx)->blind, sizeof(blind_registration), "blind             ");
    exit(1);
  }
  const uint8_t registration_request[32] = {
                                            0xac, 0x7a, 0x63, 0x30, 0xf9, 0x1d, 0x1e, 0x5c,
                                            0x87, 0x36, 0x56, 0x30, 0xc7, 0xbe, 0x58, 0x64,
                                            0x18, 0x85, 0xd5, 0x9f, 0xfe, 0x4d, 0x3f, 0x8a,
                                            0x49, 0xc0, 0x94, 0x27, 0x19, 0x93, 0x33, 0x1d};
  if(memcmp(registration_request, regreq, 32)!=0) {
    fprintf(stderr, "failed to reproduce reg req\n");
    dump(regreq, 32, "regreq");
    dump(registration_request, 32, "registration_request");
    exit(1);
  }

  // create registration response
  // prepare
  unsigned char
    rsec[OPAQUE_REGISTER_SECRET_LEN],
    resp[OPAQUE_REGISTER_PUBLIC_LEN];
  // "server_public_key": "18d5035fd0a9c1d6412226df037125901a43f4dff660c0549d402f672bcc0933"
  //const uint8_t pkS[crypto_scalarmult_BYTES] =
  //        {0x18, 0xd5, 0x03, 0x5f, 0xd0, 0xa9, 0xc1, 0xd6,
  //         0x41, 0x22, 0x26, 0xdf, 0x03, 0x71, 0x25, 0x90,
  //         0x1a, 0x43, 0xf4, 0xdf, 0xf6, 0x60, 0xc0, 0x54,
  //         0x9d, 0x40, 0x2f, 0x67, 0x2b, 0xcc, 0x09, 0x33};
  const uint8_t skS[crypto_scalarmult_SCALARBYTES] = {
     0x16, 0xeb, 0x9d, 0xc7, 0x4a, 0x3d, 0xf2, 0x03,
     0x3c, 0xd7, 0x38, 0xbf, 0x2c, 0xfb, 0x7a, 0x36,
     0x70, 0xc5, 0x69, 0xd7, 0x74, 0x9f, 0x28, 0x4b,
     0x2b, 0x24, 0x1c, 0xb2, 0x37, 0xe7, 0xd1, 0x0f};
  if(0!=opaque_CreateRegistrationResponse(regreq, skS, rsec, resp)) return 1;
  // verify test vectors
  // "registration_response": "5c7d3c70cf7478ead859bb879b37cce78baef3b9d81e04f4c790ce25f2830e2e18d5035fd0a9c1d6412226df037125901a43f4dff660c0549d402f672bcc0933"
  const uint8_t registration_response [64] = {
           0x5c, 0x7d, 0x3c, 0x70, 0xcf, 0x74, 0x78, 0xea,
           0xd8, 0x59, 0xbb, 0x87, 0x9b, 0x37, 0xcc, 0xe7,
           0x8b, 0xae, 0xf3, 0xb9, 0xd8, 0x1e, 0x04, 0xf4,
           0xc7, 0x90, 0xce, 0x25, 0xf2, 0x83, 0x0e, 0x2e,

           0x18, 0xd5, 0x03, 0x5f, 0xd0, 0xa9, 0xc1, 0xd6,
           0x41, 0x22, 0x26, 0xdf, 0x03, 0x71, 0x25, 0x90,
           0x1a, 0x43, 0xf4, 0xdf, 0xf6, 0x60, 0xc0, 0x54,
           0x9d, 0x40, 0x2f, 0x67, 0x2b, 0xcc, 0x09, 0x33};

  if(memcmp(registration_response, resp, sizeof resp)!=0) {
    fprintf(stderr,"failed to reproduce registration_response\n");
    dump(resp, sizeof resp, "resp");
    dump(registration_response, sizeof registration_response, "registration_response");
    exit(1);
  }

  // finalize request
  // prepare params
  Opaque_Ids ids={0};
  unsigned char rrec[OPAQUE_REGISTRATION_RECORD_LEN]={0};
  uint8_t ek[crypto_hash_sha512_BYTES]={0};

  if(0!=opaque_FinalizeRequest(ctx, resp, &ids, rrec, ek)) return 1;

  // verify test vectors
  // "export_key": "8408f92d282c7f4b0f5462e5206bd92937a4d53b0dcdef90afffd015c5dee44dc4dc5ad35d1681c97e2b66de09203ac359a69f1d45f8c97dbc907589177ccc24",
  const uint8_t export_key[] = {
           0x84, 0x08, 0xf9, 0x2d, 0x28, 0x2c, 0x7f, 0x4b,
           0x0f, 0x54, 0x62, 0xe5, 0x20, 0x6b, 0xd9, 0x29,
           0x37, 0xa4, 0xd5, 0x3b, 0x0d, 0xcd, 0xef, 0x90,
           0xaf, 0xff, 0xd0, 0x15, 0xc5, 0xde, 0xe4, 0x4d,
           0xc4, 0xdc, 0x5a, 0xd3, 0x5d, 0x16, 0x81, 0xc9,
           0x7e, 0x2b, 0x66, 0xde, 0x09, 0x20, 0x3a, 0xc3,
           0x59, 0xa6, 0x9f, 0x1d, 0x45, 0xf8, 0xc9, 0x7d,
           0xbc, 0x90, 0x75, 0x89, 0x17, 0x7c, 0xcc, 0x24};
  if(memcmp(export_key, ek, sizeof export_key)!=0) {
    fprintf(stderr,"failed to reproduce export_key\n");
    dump(ek, sizeof ek, "ek");
    dump(export_key, sizeof export_key, "export_key");
    exit(1);
  }
  // "registration_upload": "60c9b59f46e93a2dc8c5dd0dd101fad1838f4c4c026691e9d18d3de8f2b3940d7981498360f8f276df1dfb852a93ec4f4a0189dec5a96363296a693fc8a51fb052ae8318dac48be7e3c3cd290f7b8c12b807617b7f9399417deed00158281ac771b8f14b7a1059cdadc414c409064a22cf9e970b0ffc6f1fc6fdd539c46767750a343dd3f683692f4ed987ff286a4ece0813a4942e23477920608f261e1ab6f8727f532c9fd0cde8ec492cb76efdc855da76d0b6ccbe8a4dc0ba2709d63c4517",
  const uint8_t registration_upload[192] = {
           0x60, 0xc9, 0xb5, 0x9f, 0x46, 0xe9, 0x3a, 0x2d,
           0xc8, 0xc5, 0xdd, 0x0d, 0xd1, 0x01, 0xfa, 0xd1,
           0x83, 0x8f, 0x4c, 0x4c, 0x02, 0x66, 0x91, 0xe9,
           0xd1, 0x8d, 0x3d, 0xe8, 0xf2, 0xb3, 0x94, 0x0d,
           0x79, 0x81, 0x49, 0x83, 0x60, 0xf8, 0xf2, 0x76,
           0xdf, 0x1d, 0xfb, 0x85, 0x2a, 0x93, 0xec, 0x4f,
           0x4a, 0x01, 0x89, 0xde, 0xc5, 0xa9, 0x63, 0x63,
           0x29, 0x6a, 0x69, 0x3f, 0xc8, 0xa5, 0x1f, 0xb0,
           0x52, 0xae, 0x83, 0x18, 0xda, 0xc4, 0x8b, 0xe7,
           0xe3, 0xc3, 0xcd, 0x29, 0x0f, 0x7b, 0x8c, 0x12,
           0xb8, 0x07, 0x61, 0x7b, 0x7f, 0x93, 0x99, 0x41,
           0x7d, 0xee, 0xd0, 0x01, 0x58, 0x28, 0x1a, 0xc7,
           0x71, 0xb8, 0xf1, 0x4b, 0x7a, 0x10, 0x59, 0xcd,
           0xad, 0xc4, 0x14, 0xc4, 0x09, 0x06, 0x4a, 0x22,
           0xcf, 0x9e, 0x97, 0x0b, 0x0f, 0xfc, 0x6f, 0x1f,
           0xc6, 0xfd, 0xd5, 0x39, 0xc4, 0x67, 0x67, 0x75,
           0x0a, 0x34, 0x3d, 0xd3, 0xf6, 0x83, 0x69, 0x2f,
           0x4e, 0xd9, 0x87, 0xff, 0x28, 0x6a, 0x4e, 0xce,
           0x08, 0x13, 0xa4, 0x94, 0x2e, 0x23, 0x47, 0x79,
           0x20, 0x60, 0x8f, 0x26, 0x1e, 0x1a, 0xb6, 0xf8,
           0x72, 0x7f, 0x53, 0x2c, 0x9f, 0xd0, 0xcd, 0xe8,
           0xec, 0x49, 0x2c, 0xb7, 0x6e, 0xfd, 0xc8, 0x55,
           0xda, 0x76, 0xd0, 0xb6, 0xcc, 0xbe, 0x8a, 0x4d,
           0xc0, 0xba, 0x27, 0x09, 0xd6, 0x3c, 0x45, 0x17};
  if((sizeof rrec != sizeof registration_upload) || memcmp(registration_upload, rrec, sizeof rrec)!=0) {
    fprintf(stderr,"failed to reproduce registration_upload\n");
    dump(rrec, sizeof rrec, "rrec               ");
    dump(registration_upload, sizeof registration_upload, "registration_upload");
    exit(1);
  }

  uint8_t rec[OPAQUE_USER_RECORD_LEN];
  opaque_StoreUserRecord(rsec, rrec, rec);

  uint8_t sec[OPAQUE_USER_SESSION_SECRET_LEN+pwdU_len];
  uint8_t req[OPAQUE_USER_SESSION_PUBLIC_LEN];
  if(0!=opaque_CreateCredentialRequest(pwdU, pwdU_len, sec, req)) {
    return 1;
  }

  // "KE1": "e4e7ce5bf96ddb2924faf816774b26a0ec7a6dd9d3a5bced1f4a3675c3cfd14c804133133e7ee6836c8515752e24bb44d323fef4ead34cde967798f2e9784f69f67926bd036c5dc4971816b9376e9f64737f361ef8269c18f69f1ab555e96d4a",
  const uint8_t ke1[96] = {
           0xe4, 0xe7, 0xce, 0x5b, 0xf9, 0x6d, 0xdb, 0x29,
           0x24, 0xfa, 0xf8, 0x16, 0x77, 0x4b, 0x26, 0xa0,
           0xec, 0x7a, 0x6d, 0xd9, 0xd3, 0xa5, 0xbc, 0xed,
           0x1f, 0x4a, 0x36, 0x75, 0xc3, 0xcf, 0xd1, 0x4c,
           0x80, 0x41, 0x33, 0x13, 0x3e, 0x7e, 0xe6, 0x83,
           0x6c, 0x85, 0x15, 0x75, 0x2e, 0x24, 0xbb, 0x44,
           0xd3, 0x23, 0xfe, 0xf4, 0xea, 0xd3, 0x4c, 0xde,
           0x96, 0x77, 0x98, 0xf2, 0xe9, 0x78, 0x4f, 0x69,
           0xf6, 0x79, 0x26, 0xbd, 0x03, 0x6c, 0x5d, 0xc4,
           0x97, 0x18, 0x16, 0xb9, 0x37, 0x6e, 0x9f, 0x64,
           0x73, 0x7f, 0x36, 0x1e, 0xf8, 0x26, 0x9c, 0x18,
           0xf6, 0x9f, 0x1a, 0xb5, 0x55, 0xe9, 0x6d, 0x4a};
  if(sizeof rrec != sizeof registration_upload) {
    fprintf(stderr,"len(ke1) != len(req)\n");
    dump(req, sizeof req, "req");
    dump(ke1, sizeof ke1, "ke1");
    exit(1);
  }
  if(memcmp(ke1, req, sizeof req)!=0) {
    fprintf(stderr,"failed to reproduce ke1\n");
    dump(req, sizeof req, "req");
    dump(ke1, sizeof ke1, "ke1");
    exit(1);
  }

  uint8_t cresp[OPAQUE_SERVER_SESSION_LEN];
  uint8_t sk[OPAQUE_SHARED_SECRETBYTES];
  uint8_t authU[crypto_auth_hmacsha512_BYTES];
  uint8_t context[10]="OPAQUE-POC";
  if(0!=opaque_CreateCredentialResponse(req, rec, &ids, context, sizeof context, cresp, sk, authU)) {
    return -1;
  }

  // "session_key": "05d03f4143e5866844f7ae921d3b48f3d611e930a6c4be0993a98290085110c5a27a2e5f92aeed861b90de068a51a952aa75bf97589be7c7104a4c30cc357506"
  const uint8_t session_key[64] = {
           0x05, 0xd0, 0x3f, 0x41, 0x43, 0xe5, 0x86, 0x68,
           0x44, 0xf7, 0xae, 0x92, 0x1d, 0x3b, 0x48, 0xf3,
           0xd6, 0x11, 0xe9, 0x30, 0xa6, 0xc4, 0xbe, 0x09,
           0x93, 0xa9, 0x82, 0x90, 0x08, 0x51, 0x10, 0xc5,
           0xa2, 0x7a, 0x2e, 0x5f, 0x92, 0xae, 0xed, 0x86,
           0x1b, 0x90, 0xde, 0x06, 0x8a, 0x51, 0xa9, 0x52,
           0xaa, 0x75, 0xbf, 0x97, 0x58, 0x9b, 0xe7, 0xc7,
           0x10, 0x4a, 0x4c, 0x30, 0xcc, 0x35, 0x75, 0x06};

  if(memcmp(session_key, sk, sizeof session_key)!=0) {
    fprintf(stderr,"failed to reproduce session_key\n");
    dump(sk, sizeof sk, "sk");
    dump(session_key, sizeof session_key, "session_key");
    exit(1);
  }

  // "KE2": "1af11be29a90322dc16462d0861b1eb617611fe2f05e5e9860c164592d4f7f6254f9341ca183700f6b6acf28dbfe4a86afad788805de49f2d680ab86ff39ed7f760119ed2f12f6ec4983f2c598068057af146fd09133c75b229145b7580d53cac4ba5811552e6786837a3e03d9f7971df0dad4a04fd6a6d4164101c91137a87f4afde7dae72daf2620082f46413bbb3071767d549833bcc523acc645b571a66318b0b1f8bf4b23de35428373aa1d3a45c1e89eff88f03f9446e5dfc23b6f8394f9c5ec75a8cd571370add249e99cb8a8c43f6ef05610ac6e354642bf4fedbf696e77d4749eb304c4d74be9457c597546bc22aed699225499910fc913b3e907120638f222a1a08460f4e40d0686830d3d608ce89789489161438bf6809dbbce3a6ddb0ce8702576843b58465d6cedd4e965f3f81b92992ecec0e2137b66eff0b4
  const uint8_t ke2[] = {
           0x1a, 0xf1, 0x1b, 0xe2, 0x9a, 0x90, 0x32, 0x2d, 0xc1, 0x64, 0x62, 0xd0,
           0x86, 0x1b, 0x1e, 0xb6, 0x17, 0x61, 0x1f, 0xe2, 0xf0, 0x5e, 0x5e, 0x98,
           0x60, 0xc1, 0x64, 0x59, 0x2d, 0x4f, 0x7f, 0x62, 0x54, 0xf9, 0x34, 0x1c,
           0xa1, 0x83, 0x70, 0x0f, 0x6b, 0x6a, 0xcf, 0x28, 0xdb, 0xfe, 0x4a, 0x86,
           0xaf, 0xad, 0x78, 0x88, 0x05, 0xde, 0x49, 0xf2, 0xd6, 0x80, 0xab, 0x86,
           0xff, 0x39, 0xed, 0x7f, 0x76, 0x01, 0x19, 0xed, 0x2f, 0x12, 0xf6, 0xec,
           0x49, 0x83, 0xf2, 0xc5, 0x98, 0x06, 0x80, 0x57, 0xaf, 0x14, 0x6f, 0xd0,
           0x91, 0x33, 0xc7, 0x5b, 0x22, 0x91, 0x45, 0xb7, 0x58, 0x0d, 0x53, 0xca,
           0xc4, 0xba, 0x58, 0x11, 0x55, 0x2e, 0x67, 0x86, 0x83, 0x7a, 0x3e, 0x03,
           0xd9, 0xf7, 0x97, 0x1d, 0xf0, 0xda, 0xd4, 0xa0, 0x4f, 0xd6, 0xa6, 0xd4,
           0x16, 0x41, 0x01, 0xc9, 0x11, 0x37, 0xa8, 0x7f, 0x4a, 0xfd, 0xe7, 0xda,
           0xe7, 0x2d, 0xaf, 0x26, 0x20, 0x08, 0x2f, 0x46, 0x41, 0x3b, 0xbb, 0x30,
           0x71, 0x76, 0x7d, 0x54, 0x98, 0x33, 0xbc, 0xc5, 0x23, 0xac, 0xc6, 0x45,
           0xb5, 0x71, 0xa6, 0x63, 0x18, 0xb0, 0xb1, 0xf8, 0xbf, 0x4b, 0x23, 0xde,
           0x35, 0x42, 0x83, 0x73, 0xaa, 0x1d, 0x3a, 0x45, 0xc1, 0xe8, 0x9e, 0xff,
           0x88, 0xf0, 0x3f, 0x94, 0x46, 0xe5, 0xdf, 0xc2, 0x3b, 0x6f, 0x83, 0x94,
           0xf9, 0xc5, 0xec, 0x75, 0xa8, 0xcd, 0x57, 0x13, 0x70, 0xad, 0xd2, 0x49,
           0xe9, 0x9c, 0xb8, 0xa8, 0xc4, 0x3f, 0x6e, 0xf0, 0x56, 0x10, 0xac, 0x6e,
           0x35, 0x46, 0x42, 0xbf, 0x4f, 0xed, 0xbf, 0x69, 0x6e, 0x77, 0xd4, 0x74,
           0x9e, 0xb3, 0x04, 0xc4, 0xd7, 0x4b, 0xe9, 0x45, 0x7c, 0x59, 0x75, 0x46,
           0xbc, 0x22, 0xae, 0xd6, 0x99, 0x22, 0x54, 0x99, 0x91, 0x0f, 0xc9, 0x13,
           0xb3, 0xe9, 0x07, 0x12, 0x06, 0x38, 0xf2, 0x22, 0xa1, 0xa0, 0x84, 0x60,
           0xf4, 0xe4, 0x0d, 0x06, 0x86, 0x83, 0x0d, 0x3d, 0x60, 0x8c, 0xe8, 0x97,
           0x89, 0x48, 0x91, 0x61, 0x43, 0x8b, 0xf6, 0x80, 0x9d, 0xbb, 0xce, 0x3a,
           0x6d, 0xdb, 0x0c, 0xe8, 0x70, 0x25, 0x76, 0x84, 0x3b, 0x58, 0x46, 0x5d,
           0x6c, 0xed, 0xd4, 0xe9, 0x65, 0xf3, 0xf8, 0x1b, 0x92, 0x99, 0x2e, 0xce,
           0xc0, 0xe2, 0x13, 0x7b, 0x66, 0xef, 0xf0, 0xb4};

  if(sizeof cresp != sizeof ke2) {
    fprintf(stderr,"len(ke2) != len(resp)\n");
    dump(cresp, sizeof cresp, "resp");
    dump(ke1, sizeof ke2, "ke2");
    exit(1);
  }
  if(memcmp(ke2, cresp, sizeof cresp)!=0) {
    fprintf(stderr,"failed to reproduce ke2\n");
    dump(cresp, sizeof cresp, "resp");
    dump(ke2, sizeof ke2, "ke2");
    exit(1);
  }

  uint8_t skU[OPAQUE_SHARED_SECRETBYTES];
  uint8_t authUu[crypto_auth_hmacsha512_BYTES];
  uint8_t export_keyU[crypto_hash_sha512_BYTES];
  Opaque_Ids ids1={0};
  opaque_RecoverCredentials(cresp, sec, context, sizeof context, &ids1, skU, authUu, export_keyU);

  if(memcmp(session_key, skU, sizeof session_key)!=0) {
    fprintf(stderr,"failed to reproduce session_key\n");
    dump(skU, sizeof skU, "skU");
    dump(session_key, sizeof session_key, "session_key");
    exit(1);
  }

  if(memcmp(export_key, export_keyU, sizeof export_key)!=0) {
    fprintf(stderr,"failed to reproduce export_key\n");
    dump(export_keyU, sizeof export_keyU, "export_keyU");
    dump(export_key, sizeof export_key, "export_key");
    exit(1);
  }

  if(memcmp(authU, authUu, sizeof authU)!=0) {
    fprintf(stderr,"failed to reproduce authU\n");
    dump(authUu, sizeof authUu, "authUu");
    dump(authU, sizeof authU, "authU");
    exit(1);
  }

  return 0;
}
