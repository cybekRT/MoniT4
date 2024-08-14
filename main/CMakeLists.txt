idf_component_register(SRCS
		"main.cpp" "Display.cpp"
		"Views/Wifi.cpp"
		"Views/Dashboard.cpp"
		"Data/dashboard.c"

		"Drivers/amoled_driver.c"
		"Drivers/initSequence.c"
		"Drivers/power_driver.cpp"
		"Drivers/touch_driver.cpp"
		INCLUDE_DIRS ".")
