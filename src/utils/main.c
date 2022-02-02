#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <opaque.h>

#define MAX_PWD_LEN 1024

static void usage(const char *self) {
  fprintf(stderr, "%s - libopaque commandline frontend\n(c) Stefan Marsiske 2021-22\n\n", self);
  fprintf(stderr, "Create new OPAQUE records\n");
  fprintf(stderr, "echo -n password | %s init idU idS 3>export_key [4<skS] >record                           - create new opaque record\n", self);
  fprintf(stderr, "echo -n password | %s register >msg 3>ctx                                                 - initiate new registration\n", self);
  fprintf(stderr, "%s respond <msg >rpub 3>rsec [4<skS]                                                      - respond to new registration request\n", self);
  fprintf(stderr, "%s finalize idU idS <ctx 4<rpub 3>export_key >record                                      - finalize registration\n", self);
  fprintf(stderr, "%s store <rec 3<rsec >record                                                              - complete record\n", self);
  fprintf(stderr, "socat | %s server-reg 3>record [4<skS]                                                    - server portion of online registration\n", self);
  fprintf(stderr, "socat | %s user-reg idU idS 3< <(echo -n password) 4>export_key                           - server portion of online registration\n", self);
  fprintf(stderr, "\nRun OPAQUE\n");
  fprintf(stderr, "socat | %s server idU idS context 3<record 4>shared_key                                   - server portion of OPAQUE session\n", self);
  fprintf(stderr, "socat | %s user idU idS context 3< <(echo -n password) 4>export_key 5>shared_key [6<pkS]  - server portion of OPAQUE session\n", self);
}

static int init(const char** argv) {
  // get password
  char pwd[MAX_PWD_LEN];
  if(0!=sodium_mlock(pwd,sizeof pwd)) {
    fprintf(stderr, "error: unable to lock memory for password\n");
    return 1;
  }
  const size_t pwd_len = fread(pwd,1,sizeof(pwd),stdin);
  if(pwd_len==0) {
    fprintf(stderr,"error: no password read on stdin\n");
    return 1;
  }

  // get skS if not-autogenerated
  uint8_t *skS=NULL, skS_buf[crypto_scalarmult_SCALARBYTES];
  if(-1!=fcntl(4, F_GETFD)) {
    FILE *f = fdopen(4,"r");
    if(f==NULL) {
      perror("error: failed to open fd 4 for skS");
      return 1;
    }
    if(1!=fread(skS_buf,sizeof(skS_buf),1,f)) {
      perror("error: failed to read skS from fd");
      fclose(f);
      sodium_munlock(pwd, sizeof pwd);
      return 1;
    }
    fclose(f);
    skS=skS_buf;
  }

  FILE *ek_fd = fdopen(3,"w");
  if(ek_fd==NULL) {
    perror("failed to open fd 3 for export_key");
    return 1;
  }

  Opaque_Ids ids={strlen(argv[2]),(uint8_t*) argv[2], strlen(argv[3]),(uint8_t*) argv[3]};

  uint8_t rec[OPAQUE_USER_RECORD_LEN];
  uint8_t export_key[crypto_hash_sha512_BYTES];

  int ret = opaque_Register((const uint8_t*)pwd, pwd_len, skS, &ids, rec, export_key);
  sodium_munlock(pwd,sizeof pwd);
  if(0!=ret) {
    fprintf(stderr,"error: failed to initialize reccord\n");
    fclose(ek_fd);
    return 1;
  };

  if(1!=fwrite(export_key, sizeof export_key, 1, ek_fd)) {
    perror("failed to write export key to fd 3");
    fclose(ek_fd);
    return 1;
  }
  fclose(ek_fd);

  if(1!=fwrite(rec, sizeof rec, 1, stdout)) {
    perror("failed to write record to stdout");
    return 1;
  }

  return 0;
}

static int create_reg_req(void) {
  // get password
  char pwdU[MAX_PWD_LEN];
  if(0!=sodium_mlock(pwdU,sizeof pwdU)) {
    fprintf(stderr, "error: unable to lock memory for password\n");
    return 1;
  }
  const size_t pwdU_len = fread(pwdU,1,sizeof(pwdU),stdin);
  if(pwdU_len==0) {
    fprintf(stderr,"error: no password read on stdin\n");
    sodium_munlock(pwdU,sizeof pwdU);
    return 1;
  }

  uint8_t alpha[crypto_core_ristretto255_BYTES];
  uint8_t usr_ctx[OPAQUE_REGISTER_USER_SEC_LEN+pwdU_len];
  if(0!=sodium_mlock(usr_ctx,sizeof usr_ctx)) {
    fprintf(stderr, "error: unable to lock memory for user registration context\n");
    return 1;
  }
  fprintf(stderr, "\nopaque_CreateRegistrationRequest\n");
  if(0!=opaque_CreateRegistrationRequest((uint8_t*) pwdU, pwdU_len, usr_ctx, alpha)) {
    fprintf(stderr, "opaque_CreateRegistrationRequest failed.\n");
    sodium_munlock(pwdU,sizeof pwdU);
    sodium_munlock(usr_ctx,sizeof usr_ctx);
    return 1;
  }
  sodium_munlock(pwdU,sizeof pwdU);

  FILE *ctx_fd = fdopen(3,"w");
  if(ctx_fd==NULL) {
    perror("failed to open fd 3 for user context");
    sodium_munlock(usr_ctx,sizeof usr_ctx);
    return 1;
  }

  if(1!=fwrite(usr_ctx, sizeof usr_ctx, 1, ctx_fd)) {
    perror("failed to write user context to fd 3");
    fclose(ctx_fd);
    sodium_munlock(usr_ctx,sizeof usr_ctx);
    return 1;
  }
  sodium_munlock(usr_ctx,sizeof usr_ctx);

  if(1!=fwrite(alpha, sizeof alpha, 1, stdout)) {
    perror("failed to write message to stdout");
    return 1;
  }

  return 0;
}

static int reg_respond() {
  uint8_t alpha[crypto_core_ristretto255_BYTES];
  if(1!=fread(alpha,sizeof alpha, 1, stdin)) {
    perror("failed to read registration request from client");
    return 1;
  }

  uint8_t *skS=NULL, skS_buf[crypto_scalarmult_SCALARBYTES];
  if(-1!=fcntl(4, F_GETFD)) {
    FILE *f = fdopen(4,"r");
    if(f==NULL) {
      perror("error: failed to open fd 4 for skS");
      return 1;
    }
    if(1!=fread(skS_buf,sizeof(skS_buf),1,f)) {
      perror("error: failed to read skS from fd");
      fclose(f);
      return 1;
    }
    fclose(f);
    skS=skS_buf;
  }

  uint8_t rsec[OPAQUE_REGISTER_SECRET_LEN], rpub[OPAQUE_REGISTER_PUBLIC_LEN];
  if(0!=sodium_mlock(rsec,sizeof rsec)) {
    fprintf(stderr, "error: unable to lock memory for server registration context\n");
    return 1;
  }

  if(0!=opaque_CreateRegistrationResponse(alpha, skS, rsec, rpub)) {
    fprintf(stderr, "opaque_CreateRegistrationResponse failed.\n");
    sodium_munlock(rsec, sizeof rsec);
    return 1;
  }

  FILE *ctx_fd = fdopen(3,"w");
  if(ctx_fd==NULL) {
    perror("failed to open fd 3 for server context");
    sodium_munlock(rsec,sizeof rsec);
    return 1;
  }

  if(1!=fwrite(rsec, sizeof rsec, 1, ctx_fd)) {
    perror("failed to write server context to fd 3");
    fclose(ctx_fd);
    sodium_munlock(rsec,sizeof rsec);
    return 1;
  }
  sodium_munlock(rsec,sizeof rsec);

  if(1!=fwrite(rpub, sizeof rpub, 1, stdout)) {
    perror("failed to write server registration response to stdout");
    return 1;
  }

  return 0;
}

static int finalize(const char** argv) {
  FILE *ek_fd = fdopen(3,"w");
  if(ek_fd==NULL) {
    perror("failed to open fd 3 for export_key");
    return 1;
  }

  FILE *rpub_fd = fdopen(4,"r");
  if(rpub_fd==NULL) {
    fclose(ek_fd);
    perror("failed to open fd 4 for server response");
    return 1;
  }
  // get server response from fd 4
  uint8_t rpub[OPAQUE_REGISTER_PUBLIC_LEN];
  if(1!=fread(rpub, sizeof rpub, 1, rpub_fd)) {
    fclose(ek_fd);
    perror("failed to read server response");
    return 1;
  }
  fclose(rpub_fd);

  // get user context from stdin
  char usr_ctx[OPAQUE_REGISTER_USER_SEC_LEN+MAX_PWD_LEN];
  if(0!=sodium_mlock(usr_ctx,sizeof usr_ctx)) {
    fprintf(stderr, "error: unable to lock memory for user context\n");
    fclose(ek_fd);
    return 1;
  }

  const ssize_t pwd_len = fread(usr_ctx,1,sizeof(usr_ctx),stdin) - OPAQUE_REGISTER_USER_SEC_LEN;
  if(pwd_len<=0) {
    sodium_munlock(usr_ctx,sizeof usr_ctx);
    fclose(ek_fd);
    if(pwd_len<0) {
    fprintf(stderr,"error: corrupted user context read on stdin\n");
    } else {
      fprintf(stderr,"error: no password in user context\n");
    }
    return 1;
  }

  Opaque_Ids ids={strlen(argv[2]),(uint8_t*) argv[2], strlen(argv[3]),(uint8_t*) argv[3]};
  uint8_t export_key[crypto_hash_sha512_BYTES];
  unsigned char rrec[OPAQUE_REGISTRATION_RECORD_LEN]={0};

  if(0!=opaque_FinalizeRequest((uint8_t*) usr_ctx, rpub, &ids, rrec, export_key)) {
    fprintf(stderr, "opaque_FinalizeRequest failed.\n");
    fclose(ek_fd);
    return 1;
  }

  if(1!=fwrite(export_key, sizeof export_key, 1, ek_fd)) {
    perror("failed to write export key to fd 3");
    fclose(ek_fd);
    return 1;
  }
  fclose(ek_fd);

  if(1!=fwrite(rrec, sizeof rrec, 1, stdout)) {
    perror("failed to write record to stdout");
    return 1;
  }

  return 0;
}

static int store(void) {
  FILE *rsec_fd = fdopen(3,"r");
  if(rsec_fd==NULL) {
    perror("failed to open fd 3 for server context");
    return 1;
  }
  // get server context from fd 3
  uint8_t rsec[OPAQUE_REGISTER_SECRET_LEN];
  if(0!=sodium_mlock(rsec,sizeof rsec)) {
    fprintf(stderr, "error: unable to lock memory for rsec\n");
    return 1;
  }
  if(1!=fread(rsec, sizeof rsec, 1, rsec_fd)) {
    perror("failed to read server context");
    sodium_munlock(rsec,sizeof rsec);
    return 1;
  }
  fclose(rsec_fd);

  unsigned char rrec[OPAQUE_REGISTRATION_RECORD_LEN]={0};
  uint8_t rec[OPAQUE_USER_RECORD_LEN];

  if(1!=fread(rrec,sizeof rrec, 1, stdin)) {
    perror("failed to read user registration record");
    sodium_munlock(rsec,sizeof rsec);
    return 1;
  }

  opaque_StoreUserRecord(rsec, rrec, rec);
  sodium_munlock(rsec,sizeof rsec);

  if(1!=fwrite(rec, sizeof rec, 1, stdout)) {
    perror("failed to write final record to stdout");
    return 1;
  }

  return 0;
}

// stdin/stdout tcp to server, fd 3 password in, fd 4 export key out
static int user_reg(const char** argv) {
  FILE *ek_fd = fdopen(4,"w");
  if(ek_fd==NULL) {
    perror("failed to open fd 4 for export_key");
    return 1;
  }

  // get password
  char pwdU[MAX_PWD_LEN];
  if(0!=sodium_mlock(pwdU,sizeof pwdU)) {
    fprintf(stderr, "error: unable to lock memory for password\n");
    fclose(ek_fd);
    return 1;
  }
  FILE *fd = fdopen(3,"r");
  if(fd==NULL) {
    perror("failed to open fd 3 for reading password");
    sodium_munlock(pwdU,sizeof pwdU);
    fclose(ek_fd);
    return 1;
  }
  const size_t pwdU_len = fread(pwdU,1,sizeof(pwdU),fd);
  if(pwdU_len==0) {
    fprintf(stderr,"error: no password read on fd 3\n");
    sodium_munlock(pwdU,sizeof pwdU);
    fclose(ek_fd);
    return 1;
  }
  fclose(fd);

  uint8_t alpha[crypto_core_ristretto255_BYTES];
  uint8_t usr_ctx[OPAQUE_REGISTER_USER_SEC_LEN+pwdU_len];
  if(0!=sodium_mlock(usr_ctx,sizeof usr_ctx)) {
    fprintf(stderr, "error: unable to lock memory for user registration context\n");
    sodium_munlock(pwdU,sizeof pwdU);
    fclose(ek_fd);
    return 1;
  }

  int ret = opaque_CreateRegistrationRequest((uint8_t*) pwdU, pwdU_len, usr_ctx, alpha);
  sodium_munlock(pwdU,sizeof pwdU);
  if(0!=ret) {
    fprintf(stderr, "opaque_CreateRegistrationRequest failed.\n");
    sodium_munlock(usr_ctx,sizeof usr_ctx);
    fclose(ek_fd);
    return 1;
  }

  if(1!=fwrite(alpha, sizeof alpha, 1, stdout)) {
    perror("failed to write alpha to stdout");
    sodium_munlock(usr_ctx,sizeof usr_ctx);
    fclose(ek_fd);
    return 1;
  }
  fflush(stdout);

  // get server response from stdin
  uint8_t rpub[OPAQUE_REGISTER_PUBLIC_LEN];
  if(1!=fread(rpub, sizeof rpub, 1, stdin)) {
    perror("failed to read server response");
    sodium_munlock(usr_ctx,sizeof usr_ctx);
    fclose(ek_fd);
    return 1;
  }

  Opaque_Ids ids={strlen(argv[2]),(uint8_t*) argv[2], strlen(argv[3]),(uint8_t*) argv[3]};

  uint8_t export_key[crypto_hash_sha512_BYTES];
  uint8_t rec[OPAQUE_REGISTRATION_RECORD_LEN]={0};

  if(0!=opaque_FinalizeRequest((uint8_t*) usr_ctx, rpub, &ids, rec, export_key)) {
    fprintf(stderr, "opaque_FinalizeRequest failed.\n");
    fclose(ek_fd);
    return 1;
  }

  if(1!=fwrite(export_key, sizeof export_key, 1, ek_fd)) {
    perror("failed to write export key to fd 4");
    fclose(ek_fd);
    return 1;
  }
  fclose(ek_fd);

  if(1!=fwrite(rec, sizeof rec, 1, stdout)) {
    perror("failed to write record to stdout");
    return 1;
  }
  fflush(stdout);

  return 0;
}

// stdin/stdout tcp to client, fd 3 record out, fd 4 optional sks in
static int server_reg(void) {
  // get skS if not-autogenerated
  uint8_t *skS=NULL, skS_buf[crypto_scalarmult_SCALARBYTES];
  if(-1!=fcntl(4, F_GETFD)) {
    FILE *f = fdopen(4,"r");
    if(f==NULL) {
      perror("error: failed to open fd 4 for skS");
      return 1;
    }
    if(1!=fread(skS_buf,sizeof(skS_buf),1,f)) {
      perror("error: failed to read skS from fd");
      fclose(f);
      return 1;
    }
    fclose(f);
    skS=skS_buf;
  }

  uint8_t alpha[crypto_core_ristretto255_BYTES];
  if(1!=fread(alpha,sizeof alpha, 1, stdin)) {
    perror("failed to read registration request from client");
    return 1;
  }

  uint8_t rsec[OPAQUE_REGISTER_SECRET_LEN], rpub[OPAQUE_REGISTER_PUBLIC_LEN];
  if(0!=sodium_mlock(rsec,sizeof rsec)) {
    fprintf(stderr, "error: unable to lock memory for server registration context\n");
    return 1;
  }

  if(0!=opaque_CreateRegistrationResponse(alpha, skS, rsec, rpub)) {
    fprintf(stderr, "opaque_CreateRegistrationResponse failed.\n");
    sodium_munlock(rsec, sizeof rsec);
    return 1;
  }

  if(1!=fwrite(rpub, sizeof rpub, 1, stdout)) {
    perror("failed to write server registration response to stdout");
    return 1;
  }
  fflush(stdout);

  // server store
  uint8_t rec[OPAQUE_USER_RECORD_LEN];

  unsigned char rrec[OPAQUE_REGISTRATION_RECORD_LEN]={0};
  if(1!=fread(rrec,sizeof rrec, 1, stdin)) {
    perror("failed to read user registration record");
    sodium_munlock(rsec,sizeof rsec);
    return 1;
  }

  opaque_StoreUserRecord(rsec, rrec, rec);
  sodium_munlock(rsec,sizeof rsec);

  FILE *fd = fdopen(3,"w");
  if(NULL==fd) {
    perror("failled to open fd 3 to output final record");
    return 1;
  }
  if(1!=fwrite(rec, sizeof rec, 1, fd)) {
    perror("failed to write final record to fd 3");
    return 1;
  }
  fclose(fd);

  return 0;
}

// [user] [server] 3<password 4>export_key 5>sk [6<pkS]
static int user(const char** argv) {
  // initiate session
  // get password
  char pwdU[MAX_PWD_LEN];
  if(0!=sodium_mlock(pwdU,sizeof pwdU)) {
    fprintf(stderr, "error: unable to lock memory for password\n");
    return 1;
  }
  FILE *fd = fdopen(3,"r");
  if(fd==NULL) {
    perror("failed to open fd 3 for reading password");
    sodium_munlock(pwdU,sizeof pwdU);
    return 1;
  }
  const size_t pwdU_len = fread(pwdU,1,sizeof(pwdU),fd);
  if(pwdU_len==0) {
    fprintf(stderr,"error: no password read on fd 3\n");
    sodium_munlock(pwdU,sizeof pwdU);
    return 1;
  }
  fclose(fd);

  FILE *ek_fd = fdopen(4,"w");
  if(ek_fd==NULL) {
    perror("failed to open fd 4 for export_key");
    sodium_munlock(pwdU,sizeof pwdU);
    return 1;
  }

  FILE *sk_fd = fdopen(5,"w");
  if(ek_fd==NULL) {
    perror("failed to open fd 4 for shared_key");
    sodium_munlock(pwdU,sizeof pwdU);
    fclose(ek_fd);
    return 1;
  }

  Opaque_Ids ids={strlen(argv[2]),(uint8_t*) argv[2], strlen(argv[3]),(uint8_t*) argv[3]};
  const uint8_t *context = (const uint8_t *) argv[4];
  const size_t context_len = strlen((const char*) context);

  uint8_t sec[OPAQUE_USER_SESSION_SECRET_LEN+pwdU_len], pub[OPAQUE_USER_SESSION_PUBLIC_LEN];
  opaque_CreateCredentialRequest((uint8_t*) pwdU, pwdU_len, sec, pub);
  sodium_munlock(pwdU,sizeof pwdU);

  if(1!=fwrite(pub, sizeof pub, 1, stdout)) {
    perror("failed to write cred request to stdout");
    fclose(ek_fd);
    fclose(sk_fd);
    return 1;
  }
  fflush(stdout);

  uint8_t sk[OPAQUE_SHARED_SECRETBYTES];
  uint8_t authU[crypto_auth_hmacsha512_BYTES];
  uint8_t export_key[crypto_hash_sha512_BYTES];
  uint8_t resp[OPAQUE_SERVER_SESSION_LEN];
  if(1!=fread(resp,sizeof resp, 1, stdin)) {
    perror("failed to response from server");
    fclose(ek_fd);
    fclose(sk_fd);
    return 1;
  }

  if(0!=sodium_mlock(sk,sizeof sk)) {
    fprintf(stderr, "failed to lock memory for shared secret.\n");
    fclose(ek_fd);
    fclose(sk_fd);
    return 1;
  }
  if(0!=sodium_mlock(export_key,sizeof export_key)) {
    fprintf(stderr, "failed to lock memory for export_key.\n");
    sodium_munlock(sk, sizeof sk);
    fclose(ek_fd);
    fclose(sk_fd);
    return 1;
  }

  int ret = opaque_RecoverCredentials(resp, sec, context, context_len, &ids, sk, authU, export_key);
  if(0!=ret) {
    fprintf(stderr, "opaque_RecoverCredentials failed.\n");
    sodium_munlock(sk, sizeof sk);
    sodium_munlock(export_key, sizeof export_key);
    fclose(ek_fd);
    fclose(sk_fd);
    return 1;
  }

  ret = fwrite(export_key, sizeof export_key, 1, ek_fd);
  sodium_munlock(export_key, sizeof export_key);
  fclose(ek_fd);
  if(1!=ret) {
    perror("failed to write export key to fd 4");
    sodium_munlock(sk, sizeof sk);
    fclose(sk_fd);
    return 1;
  }

  ret = fwrite(sk, sizeof sk, 1, sk_fd);
  sodium_munlock(sk, sizeof sk);
  fclose(sk_fd);
  if(1!=ret) {
    perror("failed to shared key to fd 5");
    return 1;
  }

  if(1!=fwrite(authU, sizeof authU, 1, stdout)) {
    perror("failed to send auth token to server");
    return 1;
  }
  fflush(stdout);

  return 0;
}

// fds: 0 from client, 1 to client, 3<record, 4>sk
static int server(const char** argv) {
  Opaque_Ids ids={strlen(argv[2]),(uint8_t*) argv[2], strlen(argv[3]),(uint8_t*) argv[3]};
  const uint8_t *context = (const uint8_t *) argv[4];
  const size_t context_len = strlen((const char*) context);

  uint8_t rec[OPAQUE_USER_RECORD_LEN]; // get from fd3
  FILE *f = fdopen(3,"r");
  if(f==NULL) {
    perror("error: failed to open fd 3 for record");
    return 1;
  }
  int ret=fread(rec,sizeof(rec),1,f);
  fclose(f);
  if(1!=ret) {
    perror("error: failed to read record from fd 3");
    return 1;
  }

  uint8_t resp[OPAQUE_SERVER_SESSION_LEN];
  uint8_t pub[OPAQUE_USER_SESSION_PUBLIC_LEN]; // get from stdin
  if(1!=fread(pub,sizeof pub, 1, stdin)) {
    perror("failed to read client request from stdin");
    return 1;
  }

  uint8_t sk[OPAQUE_SHARED_SECRETBYTES];
  if(0!=sodium_mlock(sk,sizeof sk)) {
    fprintf(stderr, "failed to lock memory for shared secret.\n");
    return 1;
  }
  uint8_t authU0[crypto_auth_hmacsha512_BYTES];
  if(0!=sodium_mlock(authU0,sizeof authU0)) {
    fprintf(stderr, "failed to lock memory for expected client auth.\n");
    sodium_munlock(sk,sizeof sk);
    return 1;
  }

  if(0!=opaque_CreateCredentialResponse(pub, rec, &ids, context, context_len, resp, sk, authU0)) {
    fprintf(stderr, "opaque_CreateCredentialResponse failed.\n");
    sodium_munlock(sk,sizeof sk);
    sodium_munlock(authU0,sizeof authU0);
    return 1;
  }
  f=fdopen(4,"w");
  if(f==NULL) {
    perror("error: failed to open fd 4 for the shared secret");
    sodium_munlock(sk,sizeof sk);
    sodium_munlock(authU0,sizeof authU0);
    return 1;
  }
  ret = fwrite(sk, sizeof sk, 1, f);
  sodium_munlock(sk,sizeof sk);
  fclose(f);
  if(1!=ret) {
    fprintf(stderr, "failed to write shared secret to fd 4\n");
    sodium_munlock(authU0,sizeof authU0);
    return 1;
  }

  if(1!=fwrite(resp, sizeof resp, 1, stdout)) {
    fprintf(stderr,"failed to write server response to client\n");
    sodium_munlock(authU0,sizeof authU0);
    return 1;
  }
  fflush(stdout);

  uint8_t authU[crypto_auth_hmacsha512_BYTES]; // get from stdin
  if(1!=fread(authU, sizeof authU, 1, stdin)) {
    fprintf(stderr, "failed to read authU from stdin\n");
    sodium_munlock(authU0,sizeof authU0);
    return 1;
  }

  ret = opaque_UserAuth(authU0, authU);
  sodium_munlock(authU0,sizeof authU0);
  if(-1==ret) {
    fprintf(stderr, "failed authenticating user\n");
    return 1;
  }

  return 0;
}

int main(const int argc, const char **argv) {
  if(argc<2) {
    usage(argv[0]);
    return 0;
  }

  if(strcmp(argv[1],"init")==0) {
    if(argc<4) {
      usage(argv[0]);
      return 1;
    }
    return init(argv);
  }
  if(strcmp(argv[1],"register")==0) {
    if(argc>2) {
      usage(argv[0]);
      return 1;
    }
    return create_reg_req();
  }
  if(strcmp(argv[1],"respond")==0) {
    if(argc>2) {
      usage(argv[0]);
      return 1;
    }
    return reg_respond();
  }
  if(strcmp(argv[1],"finalize")==0) {
    if(argc<4) {
      usage(argv[0]);
      return 1;
    }
    return finalize(argv);
  }
  if(strcmp(argv[1],"store")==0) {
    if(argc<2) {
      usage(argv[0]);
      return 1;
    }
    return store();
  }
  if(strcmp(argv[1],"server-reg")==0) {
    if(argc<4) {
      usage(argv[0]);
      return 1;
    }
    return server_reg();
  }
  if(strcmp(argv[1],"user-reg")==0) {
    if(argc<4) {
      usage(argv[0]);
      return 1;
    }
    return user_reg(argv);
  }
  if(strcmp(argv[1],"user")==0) {
    if(argc<4) {
      usage(argv[0]);
      return 1;
    }
    return user(argv);
  }
  if(strcmp(argv[1],"server")==0) {
    if(argc<4) {
      usage(argv[0]);
      return 1;
    }
    return server(argv);
  }

  usage(argv[0]);
  return 1;
}
