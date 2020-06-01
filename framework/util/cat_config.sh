# ! /bin/bash
#reset all config
sudo ./reset_app
#allocate cache for COS
sudo ./allocation_app_l3cat 1 0x0000f
sudo ./allocation_app_l3cat 2 0x000f0
sudo ./allocation_app_l3cat 3 0x00f00
sudo ./allocation_app_l3cat 4 0x0f000
sudo ./allocation_app_l3cat 5 0xf0000
#associate cores with COS
sudo ./association_app 1 13
sudo ./association_app 2 14
sudo ./association_app 3 17
sudo ./association_app 4 18
sudo ./association_app 5 0 1 2 3 4 5 6 7 8 9 10 11 12 15 16 19 20 21 22 23