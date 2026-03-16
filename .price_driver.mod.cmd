savedcmd_price_driver.mod := printf '%s\n'   price_driver.o | awk '!x[$$0]++ { print("./"$$0) }' > price_driver.mod
