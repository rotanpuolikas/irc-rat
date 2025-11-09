# IRC-RAT - Internet Rat Chat (FI)

IRC-RAT on mun C:ssä kirjottama ircin kaltainen chätti, jossa clientit yhistää serverille ja voi jutella keskenään

Nimestään huolimatta, IRC-RAT ei ole RAT (Remote Access Trojan). Tai ehkä on? Ehkä jonkin kivan buffer overflown kautta saat kaapattua palvelimen haltuusi? Laita issue jos näin on.

IRC-RAT ei varmasti ole memory-safe, ja jos etsimällä etsii, löytyy tästä varmasti CVE:n arvoisia onkleemia. Älä siis käytä IRC-RAT:tiä.

Älä hostaa IRC-RAT palvelinta, ainakaan julkisena.

# IRC-RAT - Internet Rat Chat (EN)

IRC-RAT is a training project, in essence it is a very rudimentary version of IRC.

Despite the name, this is not a Remote Access Trojan. Or maybe it is? There might be a buffer overflow or memory leak or something like that through which you can control the server. If you for some reason want to find that bug, please write an issue.

IRC-RAT is not memory-safe. Do not host IRC-RAT as a public service.

You have been warned.


# TO COMPILE

On all unix devices, just use gcc

To compile on Windows, use mingw with the flags "-lws2_32 --static". This is to ensure the proper .dll files are included in the executable.


-- rotta
