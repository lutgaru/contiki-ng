trap "exit" INT TERM ERR
trap "kill 0" EXIT

#make clean
case "$1" in
    1)
        make WERROR=0 MAKE_WITH_GIFTCOFB=1 MAKE_WITH_DTLS=1 MAKE_COAP_DTLS_KEYSTORE=MAKE_COAP_DTLS_KEYSTORE_SIMPLE
        ;;
    *)
        make WERROR=0 MAKE_WITH_DTLS=1 MAKE_COAP_DTLS_KEYSTORE=MAKE_COAP_DTLS_KEYSTORE_SIMPLE
        ;;
esac
./coap-example-server.native & 
sleep 2s
coap-client -m get "coap://[fd00::302:304:506:708]/test/hello" | lolcat && coap-client -u "user" -k "password" -m get "coaps://[fd00::302:304:506:708]/test/hello" | lolcat
#cd ../coap-example-client
#make clean
# case "$1" in
#     1)
#         make WERROR=0 MAKE_WITH_GIFTCOFB=1 MAKE_WITH_DTLS=1 MAKE_COAP_DTLS_KEYSTORE=MAKE_COAP_DTLS_KEYSTORE_SIMPLE
#         ;;
#     *)
#         make WERROR=0 MAKE_WITH_DTLS=1 MAKE_COAP_DTLS_KEYSTORE=MAKE_COAP_DTLS_KEYSTORE_SIMPLE
#         ;;
# esac
# parallel -u ::: ../coap-example-server/coap-example-server.native  ./coap-example-client.native
fg
EXIT
#sudo ip addr add fd00::302:304:506:709/64 dev tun0