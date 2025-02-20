#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include <ModbusMaster.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

const char* ssid = "YourWifi"; // Your wifi name
const char* password = "Password";//Your wifi password

// MAX485 Setup
#define RX_PIN 4  // D2 (RO)
#define TX_PIN 5  // D1 (DI)
#define DE_RE_PIN 2 // D4 (DE & RE)

unsigned long lastShortRead = 0;
unsigned long lastLongRead = 0;
const unsigned long DELAY = 100;
const unsigned long SHORT_INTERVAL = 3000; // 3 seconds
const unsigned long LONG_INTERVAL = 60000; // 1 minute

SoftwareSerial modbusSerial(RX_PIN, TX_PIN);
ModbusMaster node;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Solar Monitoring</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Roboto:ital,wght@0,100..900;1,100..900&display=swap" rel="stylesheet">
    <style>
        body { 
            /* background: #f4f4f4;  */
            font-family: "Roboto", serif;
            font-optical-sizing: auto;
            font-style: normal;
            font-variation-settings: "wdth" 100;
        }
        .battery-level {
            transition: height 0.8s ease-in-out; /* Smooth height transition */
        }
        
    </style>
    <script>
        function connectWebSocket() {
            var socket = new WebSocket("ws://" + location.host + "/ws");
            socket.onopen = function () {
                console.log("WebSocket Connected");
            };

            socket.onmessage = function(event) {
                console.log(event.data);
                var data = JSON.parse(event.data);
                updateUI(data);
            };

            socket.onerror = function () {
                console.error("WebSocket Error");
                socket.close();
            };

            socket.onclose = function () {
                console.warn("WebSocket Disconnected! Reconnecting...");
                setTimeout(connectWebSocket, 3000);
            };
        }
        function formatPower(value) {
            if (value >= 1000000) {
                return (value / 1000000).toFixed(2) + " mW";
            } else if (value >= 1000) {
                return (value / 1000).toFixed(2) + " kW";
            } else {
                return value + " W";
            }
        }
        function updateBatteryLevel(level, power) {
            var batteryLevel = document.querySelector(".battery-level");
            batteryLevel.style.height = level + "%";
            batteryLevel.className = "battery-level bg-[#008ae0]";
            if (power != 0) {
                batteryLevel.classList.add("charging");
            } else {
                batteryLevel.classList.remove("charging");
            }
        }
        function setOldData(data) {
          localStorage.setItem("oldData", JSON.stringify(data));
        }
        function getOldData() {
            var old = localStorage.getItem("oldData");
            return old ? JSON.parse(old) : {};
        }
        function u(oldData, data, key) {
          if(oldData != data){
              var el = document.getElementById(key);
              el.classList.add("flash");
              setTimeout(() => {
                if(data < 0) data = data * -1;
                el.innerText = formatPower(data);
                el.classList.remove("flash");
              }, 400);
            }
        }
        function updateUI(data) {
            var oldData = getOldData();
            u(oldData.u, data.u, "p-ups");
            u(oldData.g, data.g, "p-grid");
            u(oldData.l + oldData.u, data.l + data.u, "p-load");
            u(oldData.l, data.l, "p-home");
            u(oldData.p1 + oldData.p2, data.p1 + data.p2, "p-solar");
            u(oldData.b, data.b, "p-bat");

            document.getElementById("p-pv1").innerText = formatPower(data.p1);
            document.getElementById("p-pv2").innerText = formatPower(data.p2);
            document.getElementById("b-soc").innerText = data.soc + " %";
            document.getElementById("b-temp").innerText = (data.bt/10).toFixed(1) + " °C";
            document.getElementById("b-vol").innerText = (data.bv/100).toFixed(2) + " V";
            if(data.bc < 0){
                document.getElementById("b-cur").innerText = (data.bc/10 * -1).toFixed(2) + " A";
            } else {
                document.getElementById("b-cur").innerText = (data.bc/10).toFixed(2) + " A";
            }
            document.getElementById("g-vol").innerText = (data.gv/10).toFixed(1) + " V";
            document.getElementById("g-fre").innerText = (data.gh/100).toFixed(2) + " Hz";
            document.getElementById("p-pv").innerText = formatPower(data.pv * 100);
            document.getElementById("pv1-vol").innerText = (data.p1v/10).toFixed(0) + " V";
            document.getElementById("pv1-cur").innerText = (data.p1c/100).toFixed(2) + " A";
            document.getElementById("pv2-vol").innerText = (data.p2v/10).toFixed(0) + " V";
            document.getElementById("pv2-cur").innerText = (data.p2c/100).toFixed(2) + " A";
            updateBatteryLevel(data.soc, data.b);

            if(data.p1 > 0){
                document.getElementById("n-pv1").classList.add("dot3");
            } else {
                document.getElementById("n-pv1").classList.remove("dot3");
            }
            if(data.p2 > 0){
                document.getElementById("n-pv2").classList.add("dot2");
            } else {
                document.getElementById("n-pv2").classList.remove("dot2");
            }
            if(data.p1 == 0 && data.p2 == 0){
                document.getElementById("n-pv").classList.remove("dot");
            } else {
                document.getElementById("n-pv").classList.add("dot");
            }
            if(data.l + data.u == 0){
                document.getElementById("n-load").classList.remove("dot");
            } else {
                document.getElementById("n-load").classList.add("dot");
            }

            var batI = document.getElementById("n-bat");
            if (data.b < 0) {
                batI.classList.remove("move-up");
                batI.classList.add("dot");
                batI.classList.add("move-down");
            } else if (data.b > 0) {
                batI.classList.remove("move-down");
                batI.classList.add("dot");
                batI.classList.add("move-up");
            } else {
                batI.classList.remove("dot");
            }
            var gridI = document.getElementById("n-grid");
            if (data.g < 0) {
              gridI.classList.remove("move-right");
              gridI.classList.add("dot");
              gridI.classList.add("move-left");
            } else if (data.g > 0) {
              gridI.classList.remove("move-left");
              gridI.classList.add("dot");
              gridI.classList.add("move-right");
            } else {
              gridI.classList.remove("dot");
            }
            setOldData(data);
        }
        function scaleToFit() {
            let progressBar = document.getElementById("diagram-container");
            let containerWidth = document.querySelector("body").offsetWidth;
            let originalWidth = 460; // The original size of the element

            let scaleFactor = containerWidth / originalWidth; // Calculate scale ratio
            if(scaleFactor > 1) {
                scaleFactor = 1;
            }
            progressBar.style.transform = "scale(" + scaleFactor + ")"; // Apply scaling
        }
        window.onload = function () {
            connectWebSocket();
            scaleToFit();
        };
        window.onresize = scaleToFit;
    </script>
</head>
<body >
  <div class="container flex flex-col items-center justify-center" style="margin: 0 auto;">
    <div style="font-weight: bold; padding: 5px 0; background-color: #008ae0; width: 100%; text-align: center; color: white;">LOCAL SOLAR MONITORING</div>
    <div id="diagram-container" class="diagram-container grid h-[500px] place-items-center w-full" style="margin: 0 auto; padding-right: 10px;">
      <div class="relative rounded-lg">
        <div class="h-[100px] w-[70px] rounded-sm bg-amber-950">
          <img src="data:image/png;base64,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" alt="" class="h-[100px] w-[70px] rounded-sm" />
        </div>
        <div class="absolute -top-[178px] -left-[66px] flex w-[200px] flex-col content-center">
          <div class="flex gap-2">
            <svg id="sun" x="154" y="10" width="40" height="40" viewBox="0 0 24 24">
              <path class="" fill="#057A1D" d="M11.45 2v3.55L15 3.77L11.45 2m-1 6L8 10.46l3.75 1.25L10.45 8M2 11.45L3.77 15l1.78-3.55H2M10 2H2v8c.57.17 1.17.25 1.77.25c3.58.01 6.49-2.9 6.5-6.5c-.01-.59-.1-1.18-.27-1.75m7 20v-6h-3l5-9v6h3l-5 9Z"></path>
            </svg>
            <div>
              <p class="solar-color text-xl font-bold" id="p-pv">- kW</p>
              <p class="solar-color text-[9px]">DAILY SOLAR</p>
            </div>
          </div>
          <div class="mt-3 flex gap-6">
            <div class="relative w-1/2">
              <div class="solar-color rounded-sm border-[1px] border-[#057a1c] py-1 text-center font-bold" id="p-pv2">- W</div>
              <div class="bg-re absolute right-[26px] -bottom-[35px] h-[35px] w-[20px] rounded-bl-md border-b-2 border-l-2 border-[#057a1c]">
                <div class="dot2 -ml-[5px] bg-[#057a1c]" id="n-pv2" style="--speed:3s"></div>
              </div>
              <div class="absolute text-[9px] solar-color mt-1 ml-1">
                <p id="pv1-vol">- V</p>
                <p id="pv1-cur">- A</p>
              </div>
            </div>
            <div class="relative w-1/2">
              <div class="solar-color rounded-sm border-[1px] border-[#057a1c] py-1 text-center font-bold" id="p-pv1">- W</div>
  
              <div class="absolute -bottom-[35px] left-[26px] h-[35px] w-[20px] rounded-br-md border-r-2 border-b-2 border-[#057a1c]">
                <div class="dot3 ml-[15px] bg-[#057a1c]" id="n-pv1" style="--speed:5s"></div>
              </div>
              <div class="absolute text-[9px] solar-color mt-1 mr-1 text-right right-0">
                <p id="pv2-vol">- V</p>
                <p id="pv2-cur">- A</p>
              </div>
            </div>
          </div>
  
          <div class="mt-[17px] flex justify-center">
            <div class="solar-color w-[40%] rounded-sm border-[1px] border-[#057a1c] py-1 text-center font-bold"><span id="p-solar">- kW</span></div>
          </div>
        </div>
        <div class="battery-color absolute -bottom-[174px] -left-[166px] flex w-[400px] flex-col items-center">
          <div class="solar-color battery-color rounded-sm border-[1px] border-[#008ae0] px-4 py-1 text-center font-bold"><span id="p-bat">- kW</span></div>
          <div class="mt-5 flex justify-center">
            <div>
              <p id="b-vol" class="font-bold">- V</p>
              <p class="mb-1 text-[9px]">BATTERY VOLTAGE</p>
              <p id="b-cur" class="font-bold">- A</p>
              <p class="text-[9px]">BATTERY CURRENT</p>
            </div>
            <div class="relative ml-5 flex h-20 w-12 flex-col justify-end rounded-lg border-4 border-[#008ae0]">
              <div class="absolute -top-2 left-1/2 h-2 w-4 -translate-x-1/2 transform rounded-lg bg-[#008ae0]"></div>
              <div class="battery-level h-0 w-full transition-all duration-500 ease-in-out"></div>
            </div>
            <div class="ml-5">
              <p id="b-soc" class="font-bold">- %</p>
              <p class="mb-1 text-[9px]">STATE OF CHARGE</p>
              <p id="b-temp" class="font-bold">- °C</p>
              <p class="text-[9px]">TEMPERATURE</p>
            </div>
          </div>
        </div>
        <div class="grid-color absolute -top-[50px] -left-[180px] flex h-[200px] flex-col justify-center">
          <p class="text-left font-bold" id="g-vol">- V</p>
          <p class="text-left text-[9px]">GRID VOLTAGE</p>
          <div class="relative flex items-center justify-between">
            <svg id="transmission_on" x="-0.5" y="187.5" width="64.5" height="64.5" viewBox="0 0 24 24">
              <path class="" fill="#C70000" display="" d="m8.28 5.45l-1.78-.9L7.76 2h8.47l1.27 2.55l-1.78.89L15 4H9l-.72 1.45M18.62 8h-4.53l-.79-3h-2.6l-.79 3H5.38L4.1 10.55l1.79.89l.73-1.44h10.76l.72 1.45l1.79-.89L18.62 8m-.85 14H15.7l-.24-.9L12 15.9l-3.47 5.2l-.23.9H6.23l2.89-11h2.07l-.36 1.35L12 14.1l1.16-1.75l-.35-1.35h2.07l2.89 11m-6.37-7l-.9-1.35l-1.18 4.48L11.4 15m3.28 3.12l-1.18-4.48l-.9 1.36l2.08 3.12Z"></path>
            </svg>
            <p class="solar-color grid-color w-[80px] rounded-sm border-[1px] border-[#c70000] py-1 text-center font-bold flip" style="width: 80px;" id="p-grid">- W</p>
          </div>
          <p class="text-left font-bold" id="g-fre">- Hz</p>
          <p class="text-left text-[9px]">GRID FREQUENCY</p>
        </div>
        <div class="home-color absolute -top-[50px] -right-[203px] flex h-[200px] items-center justify-between" style="padding-right: 10px;">
          <div>
            <p class="solar-color home-color w-[80px] rounded-sm border-[1px] border-[#ff7b00] py-1 text-center font-bold" style="width: 80px;"><span id="p-load" class="flash">- W</span></p>
          </div>
          <div class="relative flex flex-col items-center justify-center gap-1">
            <p class="text-[9px]">UPS</p>
            <p class="solar-color home-color rounded-sm border-[1px] border-[#ff7b00] px-2 py-1 text-center font-bold "><span id="p-ups" class="flash">- W</span></p>
            <div class="relative my-5 flex items-center justify-between">
              <svg id="essen" viewBox="0 0 24 24" x="402" y="177.5" width="79" height="79">
                <defs>
                  <linearGradient x1="0%" x2="0%" y1="100%" y2="0%" id="Lg-1739720801079">
                    <stop offset="0%" stop-color="#c70000"></stop>
                    <stop offset="0%" stop-color="#c70000"></stop>
                    <stop offset="0%" stop-color="#c70000"></stop>
                    <stop offset="0%" stop-color="#c70000"></stop>
                    <stop offset="0%" stop-color="#c70000"></stop>
                    <stop offset="100%" stop-color="#c70000"></stop>
                  </linearGradient>
                </defs>
                <path fill="#ff7b00" d="m15 13l-4 4v-3H2v-2h9V9l4 4M5 20v-4h2v2h10v-7.81l-5-4.5L7.21 10H4.22L12 3l10 9h-3v8H5Z"></path>
              </svg>
            </div>
            <p class="solar-color home-color rounded-sm border-[1px] border-[#ff7b00] px-2 py-1 text-center font-bold" id="p-home">- kW</p>
            <p class="text-[9px]">HOME</p>
  
            <div class="absolute top-[51px] right-[50%] h-[30px] w-[2px] bg-[#ff7b00]"></div>
            <div class="absolute right-[50%] bottom-[51px] h-[30px] w-[2px] bg-[#ff7b00]"></div>
          </div>
        </div>
  
        <div class="absolute -top-[40px] left-[33px] h-[40px] w-[2px] bg-[#057a1c]">
          <div class="dot move-down -ml-[3px] bg-[#057a1c]" id="n-pv" style="--speed:1.5s"></div>
        </div>
        <div class="absolute -bottom-[40px] left-[33px] h-[40px] w-[2px] bg-[#008ae0]">
          <div class="dot move-down -ml-[3px] bg-[#008ae0]" style="--speed:3s" id="n-bat"></div>
        </div>
        <div class="absolute top-[50px] -left-[35px] h-[2px] w-[35px] bg-[#c70000]">
          <div class="dot move-right n-grid -mt-[3px] bg-[#c70000]" style="--speed:4s" id="n-grid"></div>
        </div>
        <div class="absolute top-[50px] -right-[35px] h-[2px] w-[35px] bg-[#ff7b00]">
          <div class="dot move-right -mt-[3px] bg-[#ff7b00]" style="--speed:2s" id="n-load"></div>
        </div>
      </div>
    </div>
  </div>
</body>
</html>
<style>
/*! tailwindcss v4.0.6 | MIT License | https://tailwindcss.com */@layer theme,base,components,utilities; @layer theme{:host,:root{--font-sans:ui-sans-serif,system-ui,sans-serif,'Apple Color Emoji','Segoe UI Emoji','Segoe UI Symbol','Noto Color Emoji';--font-serif:ui-serif,Georgia,Cambria,'Times New Roman',Times,serif;--font-mono:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,'Liberation Mono','Courier New',monospace;--color-red-50:oklch(0.971 0.013 17.38);--color-red-100:oklch(0.936 0.032 17.717);--color-red-200:oklch(0.885 0.062 18.334);--color-red-300:oklch(0.808 0.114 19.571);--color-red-400:oklch(0.704 0.191 22.216);--color-red-500:oklch(0.637 0.237 25.331);--color-red-600:oklch(0.577 0.245 27.325);--color-red-700:oklch(0.505 0.213 27.518);--color-red-800:oklch(0.444 0.177 26.899);--color-red-900:oklch(0.396 0.141 25.723);--color-red-950:oklch(0.258 0.092 26.042);--color-orange-50:oklch(0.98 0.016 73.684);--color-orange-100:oklch(0.954 0.038 75.164);--color-orange-200:oklch(0.901 0.076 70.697);--color-orange-300:oklch(0.837 0.128 66.29);--color-orange-400:oklch(0.75 0.183 55.934);--color-orange-500:oklch(0.705 0.213 47.604);--color-orange-600:oklch(0.646 0.222 41.116);--color-orange-700:oklch(0.553 0.195 38.402);--color-orange-800:oklch(0.47 0.157 37.304);--color-orange-900:oklch(0.408 0.123 38.172);--color-orange-950:oklch(0.266 0.079 36.259);--color-amber-50:oklch(0.987 0.022 95.277);--color-amber-100:oklch(0.962 0.059 95.617);--color-amber-200:oklch(0.924 0.12 95.746);--color-amber-300:oklch(0.879 0.169 91.605);--color-amber-400:oklch(0.828 0.189 84.429);--color-amber-500:oklch(0.769 0.188 70.08);--color-amber-600:oklch(0.666 0.179 58.318);--color-amber-700:oklch(0.555 0.163 48.998);--color-amber-800:oklch(0.473 0.137 46.201);--color-amber-900:oklch(0.414 0.112 45.904);--color-amber-950:oklch(0.279 0.077 45.635);--color-yellow-50:oklch(0.987 0.026 102.212);--color-yellow-100:oklch(0.973 0.071 103.193);--color-yellow-200:oklch(0.945 0.129 101.54);--color-yellow-300:oklch(0.905 0.182 98.111);--color-yellow-400:oklch(0.852 0.199 91.936);--color-yellow-500:oklch(0.795 0.184 86.047);--color-yellow-600:oklch(0.681 0.162 75.834);--color-yellow-700:oklch(0.554 0.135 66.442);--color-yellow-800:oklch(0.476 0.114 61.907);--color-yellow-900:oklch(0.421 0.095 57.708);--color-yellow-950:oklch(0.286 0.066 53.813);--color-lime-50:oklch(0.986 0.031 120.757);--color-lime-100:oklch(0.967 0.067 122.328);--color-lime-200:oklch(0.938 0.127 124.321);--color-lime-300:oklch(0.897 0.196 126.665);--color-lime-400:oklch(0.841 0.238 128.85);--color-lime-500:oklch(0.768 0.233 130.85);--color-lime-600:oklch(0.648 0.2 131.684);--color-lime-700:oklch(0.532 0.157 131.589);--color-lime-800:oklch(0.453 0.124 130.933);--color-lime-900:oklch(0.405 0.101 131.063);--color-lime-950:oklch(0.274 0.072 132.109);--color-green-50:oklch(0.982 0.018 155.826);--color-green-100:oklch(0.962 0.044 156.743);--color-green-200:oklch(0.925 0.084 155.995);--color-green-300:oklch(0.871 0.15 154.449);--color-green-400:oklch(0.792 0.209 151.711);--color-green-500:oklch(0.723 0.219 149.579);--color-green-600:oklch(0.627 0.194 149.214);--color-green-700:oklch(0.527 0.154 150.069);--color-green-800:oklch(0.448 0.119 151.328);--color-green-900:oklch(0.393 0.095 152.535);--color-green-950:oklch(0.266 0.065 152.934);--color-emerald-50:oklch(0.979 0.021 166.113);--color-emerald-100:oklch(0.95 0.052 163.051);--color-emerald-200:oklch(0.905 0.093 164.15);--color-emerald-300:oklch(0.845 0.143 164.978);--color-emerald-400:oklch(0.765 0.177 163.223);--color-emerald-500:oklch(0.696 0.17 162.48);--color-emerald-600:oklch(0.596 0.145 163.225);--color-emerald-700:oklch(0.508 0.118 165.612);--color-emerald-800:oklch(0.432 0.095 166.913);--color-emerald-900:oklch(0.378 0.077 168.94);--color-emerald-950:oklch(0.262 0.051 172.552);--color-teal-50:oklch(0.984 0.014 180.72);--color-teal-100:oklch(0.953 0.051 180.801);--color-teal-200:oklch(0.91 0.096 180.426);--color-teal-300:oklch(0.855 0.138 181.071);--color-teal-400:oklch(0.777 0.152 181.912);--color-teal-500:oklch(0.704 0.14 182.503);--color-teal-600:oklch(0.6 0.118 184.704);--color-teal-700:oklch(0.511 0.096 186.391);--color-teal-800:oklch(0.437 0.078 188.216);--color-teal-900:oklch(0.386 0.063 188.416);--color-teal-950:oklch(0.277 0.046 192.524);--color-cyan-50:oklch(0.984 0.019 200.873);--color-cyan-100:oklch(0.956 0.045 203.388);--color-cyan-200:oklch(0.917 0.08 205.041);--color-cyan-300:oklch(0.865 0.127 207.078);--color-cyan-400:oklch(0.789 0.154 211.53);--color-cyan-500:oklch(0.715 0.143 215.221);--color-cyan-600:oklch(0.609 0.126 221.723);--color-cyan-700:oklch(0.52 0.105 223.128);--color-cyan-800:oklch(0.45 0.085 224.283);--color-cyan-900:oklch(0.398 0.07 227.392);--color-cyan-950:oklch(0.302 0.056 229.695);--color-sky-50:oklch(0.977 0.013 236.62);--color-sky-100:oklch(0.951 0.026 236.824);--color-sky-200:oklch(0.901 0.058 230.902);--color-sky-300:oklch(0.828 0.111 230.318);--color-sky-400:oklch(0.746 0.16 232.661);--color-sky-500:oklch(0.685 0.169 237.323);--color-sky-600:oklch(0.588 0.158 241.966);--color-sky-700:oklch(0.5 0.134 242.749);--color-sky-800:oklch(0.443 0.11 240.79);--color-sky-900:oklch(0.391 0.09 240.876);--color-sky-950:oklch(0.293 0.066 243.157);--color-blue-50:oklch(0.97 0.014 254.604);--color-blue-100:oklch(0.932 0.032 255.585);--color-blue-200:oklch(0.882 0.059 254.128);--color-blue-300:oklch(0.809 0.105 251.813);--color-blue-400:oklch(0.707 0.165 254.624);--color-blue-500:oklch(0.623 0.214 259.815);--color-blue-600:oklch(0.546 0.245 262.881);--color-blue-700:oklch(0.488 0.243 264.376);--color-blue-800:oklch(0.424 0.199 265.638);--color-blue-900:oklch(0.379 0.146 265.522);--color-blue-950:oklch(0.282 0.091 267.935);--color-indigo-50:oklch(0.962 0.018 272.314);--color-indigo-100:oklch(0.93 0.034 272.788);--color-indigo-200:oklch(0.87 0.065 274.039);--color-indigo-300:oklch(0.785 0.115 274.713);--color-indigo-400:oklch(0.673 0.182 276.935);--color-indigo-500:oklch(0.585 0.233 277.117);--color-indigo-600:oklch(0.511 0.262 276.966);--color-indigo-700:oklch(0.457 0.24 277.023);--color-indigo-800:oklch(0.398 0.195 277.366);--color-indigo-900:oklch(0.359 0.144 278.697);--color-indigo-950:oklch(0.257 0.09 281.288);--color-violet-50:oklch(0.969 0.016 293.756);--color-violet-100:oklch(0.943 0.029 294.588);--color-violet-200:oklch(0.894 0.057 293.283);--color-violet-300:oklch(0.811 0.111 293.571);--color-violet-400:oklch(0.702 0.183 293.541);--color-violet-500:oklch(0.606 0.25 292.717);--color-violet-600:oklch(0.541 0.281 293.009);--color-violet-700:oklch(0.491 0.27 292.581);--color-violet-800:oklch(0.432 0.232 292.759);--color-violet-900:oklch(0.38 0.189 293.745);--color-violet-950:oklch(0.283 0.141 291.089);--color-purple-50:oklch(0.977 0.014 308.299);--color-purple-100:oklch(0.946 0.033 307.174);--color-purple-200:oklch(0.902 0.063 306.703);--color-purple-300:oklch(0.827 0.119 306.383);--color-purple-400:oklch(0.714 0.203 305.504);--color-purple-500:oklch(0.627 0.265 303.9);--color-purple-600:oklch(0.558 0.288 302.321);--color-purple-700:oklch(0.496 0.265 301.924);--color-purple-800:oklch(0.438 0.218 303.724);--color-purple-900:oklch(0.381 0.176 304.987);--color-purple-950:oklch(0.291 0.149 302.717);--color-fuchsia-50:oklch(0.977 0.017 320.058);--color-fuchsia-100:oklch(0.952 0.037 318.852);--color-fuchsia-200:oklch(0.903 0.076 319.62);--color-fuchsia-300:oklch(0.833 0.145 321.434);--color-fuchsia-400:oklch(0.74 0.238 322.16);--color-fuchsia-500:oklch(0.667 0.295 322.15);--color-fuchsia-600:oklch(0.591 0.293 322.896);--color-fuchsia-700:oklch(0.518 0.253 323.949);--color-fuchsia-800:oklch(0.452 0.211 324.591);--color-fuchsia-900:oklch(0.401 0.17 325.612);--color-fuchsia-950:oklch(0.293 0.136 325.661);--color-pink-50:oklch(0.971 0.014 343.198);--color-pink-100:oklch(0.948 0.028 342.258);--color-pink-200:oklch(0.899 0.061 343.231);--color-pink-300:oklch(0.823 0.12 346.018);--color-pink-400:oklch(0.718 0.202 349.761);--color-pink-500:oklch(0.656 0.241 354.308);--color-pink-600:oklch(0.592 0.249 0.584);--color-pink-700:oklch(0.525 0.223 3.958);--color-pink-800:oklch(0.459 0.187 3.815);--color-pink-900:oklch(0.408 0.153 2.432);--color-pink-950:oklch(0.284 0.109 3.907);--color-rose-50:oklch(0.969 0.015 12.422);--color-rose-100:oklch(0.941 0.03 12.58);--color-rose-200:oklch(0.892 0.058 10.001);--color-rose-300:oklch(0.81 0.117 11.638);--color-rose-400:oklch(0.712 0.194 13.428);--color-rose-500:oklch(0.645 0.246 16.439);--color-rose-600:oklch(0.586 0.253 17.585);--color-rose-700:oklch(0.514 0.222 16.935);--color-rose-800:oklch(0.455 0.188 13.697);--color-rose-900:oklch(0.41 0.159 10.272);--color-rose-950:oklch(0.271 0.105 12.094);--color-slate-50:oklch(0.984 0.003 247.858);--color-slate-100:oklch(0.968 0.007 247.896);--color-slate-200:oklch(0.929 0.013 255.508);--color-slate-300:oklch(0.869 0.022 252.894);--color-slate-400:oklch(0.704 0.04 256.788);--color-slate-500:oklch(0.554 0.046 257.417);--color-slate-600:oklch(0.446 0.043 257.281);--color-slate-700:oklch(0.372 0.044 257.287);--color-slate-800:oklch(0.279 0.041 260.031);--color-slate-900:oklch(0.208 0.042 265.755);--color-slate-950:oklch(0.129 0.042 264.695);--color-gray-50:oklch(0.985 0.002 247.839);--color-gray-100:oklch(0.967 0.003 264.542);--color-gray-200:oklch(0.928 0.006 264.531);--color-gray-300:oklch(0.872 0.01 258.338);--color-gray-400:oklch(0.707 0.022 261.325);--color-gray-500:oklch(0.551 0.027 264.364);--color-gray-600:oklch(0.446 0.03 256.802);--color-gray-700:oklch(0.373 0.034 259.733);--color-gray-800:oklch(0.278 0.033 256.848);--color-gray-900:oklch(0.21 0.034 264.665);--color-gray-950:oklch(0.13 0.028 261.692);--color-zinc-50:oklch(0.985 0 0);--color-zinc-100:oklch(0.967 0.001 286.375);--color-zinc-200:oklch(0.92 0.004 286.32);--color-zinc-300:oklch(0.871 0.006 286.286);--color-zinc-400:oklch(0.705 0.015 286.067);--color-zinc-500:oklch(0.552 0.016 285.938);--color-zinc-600:oklch(0.442 0.017 285.786);
--color-zinc-700:oklch(0.37 0.013 285.805);--color-zinc-800:oklch(0.274 0.006 286.033);--color-zinc-900:oklch(0.21 0.006 285.885);--color-zinc-950:oklch(0.141 0.005 285.823);--color-neutral-50:oklch(0.985 0 0);--color-neutral-100:oklch(0.97 0 0);--color-neutral-200:oklch(0.922 0 0);--color-neutral-300:oklch(0.87 0 0);--color-neutral-400:oklch(0.708 0 0);--color-neutral-500:oklch(0.556 0 0);--color-neutral-600:oklch(0.439 0 0);--color-neutral-700:oklch(0.371 0 0);--color-neutral-800:oklch(0.269 0 0);--color-neutral-900:oklch(0.205 0 0);--color-neutral-950:oklch(0.145 0 0);--color-stone-50:oklch(0.985 0.001 106.423);--color-stone-100:oklch(0.97 0.001 106.424);--color-stone-200:oklch(0.923 0.003 48.717);--color-stone-300:oklch(0.869 0.005 56.366);--color-stone-400:oklch(0.709 0.01 56.259);--color-stone-500:oklch(0.553 0.013 58.071);--color-stone-600:oklch(0.444 0.011 73.639);--color-stone-700:oklch(0.374 0.01 67.558);--color-stone-800:oklch(0.268 0.007 34.298);--color-stone-900:oklch(0.216 0.006 56.043);--color-stone-950:oklch(0.147 0.004 49.25);--color-black:#000;--color-white:#fff;--spacing:0.25rem;--breakpoint-sm:40rem;--breakpoint-md:48rem;--breakpoint-lg:64rem;--breakpoint-xl:80rem;--breakpoint-2xl:96rem;--container-3xs:16rem;--container-2xs:18rem;--container-xs:20rem;--container-sm:24rem;--container-md:28rem;--container-lg:32rem;--container-xl:36rem;--container-2xl:42rem;--container-3xl:48rem;--container-4xl:56rem;--container-5xl:64rem;--container-6xl:72rem;--container-7xl:80rem;--text-xs:0.75rem;--text-xs--line-height:calc(1 / 0.75);--text-sm:0.875rem;--text-sm--line-height:calc(1.25 / 0.875);--text-base:1rem;--text-base--line-height:calc(1.5 / 1);--text-lg:1.125rem;--text-lg--line-height:calc(1.75 / 1.125);--text-xl:1.25rem;--text-xl--line-height:calc(1.75 / 1.25);--text-2xl:1.5rem;--text-2xl--line-height:calc(2 / 1.5);--text-3xl:1.875rem;--text-3xl--line-height:calc(2.25 / 1.875);--text-4xl:2.25rem;--text-4xl--line-height:calc(2.5 / 2.25);--text-5xl:3rem;--text-5xl--line-height:1;--text-6xl:3.75rem;--text-6xl--line-height:1;--text-7xl:4.5rem;--text-7xl--line-height:1;--text-8xl:6rem;--text-8xl--line-height:1;--text-9xl:8rem;--text-9xl--line-height:1;--font-weight-thin:100;--font-weight-extralight:200;--font-weight-light:300;--font-weight-normal:400;--font-weight-medium:500;--font-weight-semibold:600;--font-weight-bold:700;--font-weight-extrabold:800;--font-weight-black:900;--tracking-tighter:-0.05em;--tracking-tight:-0.025em;--tracking-normal:0em;--tracking-wide:0.025em;--tracking-wider:0.05em;--tracking-widest:0.1em;--leading-tight:1.25;--leading-snug:1.375;--leading-normal:1.5;--leading-relaxed:1.625;--leading-loose:2;--radius-xs:0.125rem;--radius-sm:0.25rem;--radius-md:0.375rem;--radius-lg:0.5rem;--radius-xl:0.75rem;--radius-2xl:1rem;--radius-3xl:1.5rem;--radius-4xl:2rem;--shadow-2xs:0 1px rgb(0 0 0 / 0.05);--shadow-xs:0 1px 2px 0 rgb(0 0 0 / 0.05);--shadow-sm:0 1px 3px 0 rgb(0 0 0 / 0.1),0 1px 2px -1px rgb(0 0 0 / 0.1);--shadow-md:0 4px 6px -1px rgb(0 0 0 / 0.1),0 2px 4px -2px rgb(0 0 0 / 0.1);--shadow-lg:0 10px 15px -3px rgb(0 0 0 / 0.1),0 4px 6px -4px rgb(0 0 0 / 0.1);--shadow-xl:0 20px 25px -5px rgb(0 0 0 / 0.1),0 8px 10px -6px rgb(0 0 0 / 0.1);--shadow-2xl:0 25px 50px -12px rgb(0 0 0 / 0.25);--inset-shadow-2xs:inset 0 1px rgb(0 0 0 / 0.05);--inset-shadow-xs:inset 0 1px 1px rgb(0 0 0 / 0.05);--inset-shadow-sm:inset 0 2px 4px rgb(0 0 0 / 0.05);--drop-shadow-xs:0 1px 1px rgb(0 0 0 / 0.05);--drop-shadow-sm:0 1px 2px rgb(0 0 0 / 0.15);--drop-shadow-md:0 3px 3px rgb(0 0 0 / 0.12);--drop-shadow-lg:0 4px 4px rgb(0 0 0 / 0.15);--drop-shadow-xl:0 9px 7px rgb(0 0 0 / 0.1);--drop-shadow-2xl:0 25px 25px rgb(0 0 0 / 0.15);--ease-in:cubic-bezier(0.4, 0, 1, 1);--ease-out:cubic-bezier(0, 0, 0.2, 1);--ease-in-out:cubic-bezier(0.4, 0, 0.2, 1);--animate-spin:spin 1s linear infinite;--animate-ping:ping 1s cubic-bezier(0, 0, 0.2, 1) infinite;--animate-pulse:pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite;--animate-bounce:bounce 1s infinite;--blur-xs:4px;--blur-sm:8px;--blur-md:12px;--blur-lg:16px;--blur-xl:24px;--blur-2xl:40px;--blur-3xl:64px;--perspective-dramatic:100px;--perspective-near:300px;--perspective-normal:500px;--perspective-midrange:800px;--perspective-distant:1200px;--aspect-video:16/9;--default-transition-duration:150ms;--default-transition-timing-function:cubic-bezier(0.4, 0, 0.2, 1);--default-font-family:var(--font-sans);--default-font-feature-settings:var(--font-sans--font-feature-settings);--default-font-variation-settings:var(--font-sans--font-variation-settings);--default-mono-font-family:var(--font-mono);--default-mono-font-feature-settings:var(--font-mono--font-feature-settings);--default-mono-font-variation-settings:var(--font-mono--font-variation-settings)}}@layer base{progress,sub,sup{vertical-align:baseline}a,hr{color:inherit}*,::after,::backdrop,::before,::file-selector-button{box-sizing:border-box;margin:0;padding:0;border:0 solid}:host,html{line-height:1.5;-webkit-text-size-adjust:100%;tab-size:4;font-family:var( --default-font-family, ui-sans-serif, system-ui, sans-serif, 'Apple Color Emoji', 'Segoe UI Emoji', 'Segoe UI Symbol', 'Noto Color Emoji' );font-feature-settings:var(--default-font-feature-settings,normal);font-variation-settings:var(--default-font-variation-settings,normal);-webkit-tap-highlight-color:transparent}body{line-height:inherit}hr{height:0;border-top-width:1px}abbr:where([title]){-webkit-text-decoration:underline dotted;text-decoration:underline dotted}h1,h2,h3,h4,h5,h6{font-size:inherit;font-weight:inherit}a{-webkit-text-decoration:inherit;text-decoration:inherit}b,strong{font-weight:bolder}code,kbd,pre,samp{font-family:var( --default-mono-font-family, ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, 'Liberation Mono', 'Courier New', monospace );font-feature-settings:var(--default-mono-font-feature-settings,normal);font-variation-settings:var(--default-mono-font-variation-settings,normal);font-size:1em}small{font-size:80%}sub,sup{font-size:75%;line-height:0;position:relative}sub{bottom:-.25em}sup{top:-.5em}table{text-indent:0;border-color:inherit;border-collapse:collapse}:-moz-focusring{outline:auto}summary{display:list-item}menu,ol,ul{list-style:none}audio,canvas,embed,iframe,img,object,svg,video{display:block;vertical-align:middle}img,video{max-width:100%;height:auto}::file-selector-button,button,input,optgroup,select,textarea{font:inherit;font-feature-settings:inherit;font-variation-settings:inherit;letter-spacing:inherit;color:inherit;border-radius:0;background-color:transparent;opacity:1}:where(select:is([multiple],[size])) optgroup{font-weight:bolder}:where(select:is([multiple],[size])) optgroup option{padding-inline-start:20px}::file-selector-button{margin-inline-end:4px}::placeholder{opacity:1;color:color-mix(in oklab,currentColor 50%,transparent)}textarea{resize:vertical}::-webkit-search-decoration{-webkit-appearance:none}::-webkit-date-and-time-value{min-height:1lh;text-align:inherit}::-webkit-datetime-edit{display:inline-flex}::-webkit-datetime-edit-fields-wrapper{padding:0}::-webkit-datetime-edit,::-webkit-datetime-edit-day-field,::-webkit-datetime-edit-hour-field,::-webkit-datetime-edit-meridiem-field,::-webkit-datetime-edit-millisecond-field,::-webkit-datetime-edit-minute-field,::-webkit-datetime-edit-month-field,::-webkit-datetime-edit-second-field,::-webkit-datetime-edit-year-field{padding-block:0}:-moz-ui-invalid{box-shadow:none}::file-selector-button,button,input:where([type=button],[type=reset],[type=submit]){appearance:button}::-webkit-inner-spin-button,::-webkit-outer-spin-button{height:auto}[hidden]:where(:not([hidden=until-found])){display:none!important}}.border-4,.border-\[1px\]{border-style:var(--tw-border-style)}@layer utilities{.absolute{position:absolute}.relative{position:relative}.-top-2{top:calc(var(--spacing) * -2)}.-top-\[40px\]{top:calc(40px * -1)}.-top-\[50px\]{top:calc(50px * -1)}.-top-\[178px\]{top:calc(178px * -1)}.top-\[50px\]{top:50px}.top-\[51px\]{top:51px}.-right-\[35px\]{right:calc(35px * -1)}.-right-\[203px\]{right:calc(203px * -1)}.right-0{right:calc(var(--spacing) * 0)}.right-\[26px\]{right:26px}.right-\[50\%\]{right:50%}.-bottom-\[35px\]{bottom:calc(35px * -1)}.-bottom-\[40px\]{bottom:calc(40px * -1)}.-bottom-\[174px\]{bottom:calc(174px * -1)}.bottom-\[51px\]{bottom:51px}.-left-\[35px\]{left:calc(35px * -1)}.-left-\[66px\]{left:calc(66px * -1)}.-left-\[166px\]{left:calc(166px * -1)}.-left-\[180px\]{left:calc(180px * -1)}.left-1\/2{left:calc(1/2 * 100%)}.left-\[26px\]{left:26px}.left-\[33px\]{left:33px}.container{width:100%}}.my-5{margin-block:calc(var(--spacing) * 5)}.-mt-\[3px\]{margin-top:calc(3px * -1)}.mt-1{margin-top:calc(var(--spacing) * 1)}.mt-3{margin-top:calc(var(--spacing) * 3)}.mt-5{margin-top:calc(var(--spacing) * 5)}.mt-\[17px\]{margin-top:17px}.mr-1{margin-right:calc(var(--spacing) * 1)}.mb-1{margin-bottom:calc(var(--spacing) * 1)}.-ml-\[3px\]{margin-left:calc(3px * -1)}.-ml-\[5px\]{margin-left:calc(5px * -1)}.ml-1{margin-left:calc(var(--spacing) * 1)}.ml-5{margin-left:calc(var(--spacing) * 5)}.ml-\[15px\]{margin-left:15px}.flex{display:flex}.grid{display:grid}.h-0{height:calc(var(--spacing) * 0)}.h-2{height:calc(var(--spacing) * 2)}.h-20{height:calc(var(--spacing) * 20)}.h-\[2px\]{height:2px}.h-\[30px\]{height:30px}.h-\[35px\]{height:35px}.h-\[40px\]{height:40px}.h-\[100px\]{height:100px}.h-\[200px\]{height:200px}.h-\[500px\]{height:500px}.w-1\/2{width:calc(1/2 * 100%)}.w-4{width:calc(var(--spacing) * 4)}.w-12{width:calc(var(--spacing) * 12)}.w-\[40\%\]{width:40%}.w-\[70px\]{width:70px}.w-\[200px\]{width:200px}.w-\[20px\]{width:20px}.w-\[2px\]{width:2px}.w-\[35px\]{width:35px}.w-\[80px\]{width:80px}.w-\[400px\]{width:400px}.w-full{width:100%}.-translate-x-1\/2{--tw-translate-x:calc(calc(1/2 * 100%) * -1);translate:var(--tw-translate-x) var(--tw-translate-y)}.transform{transform:var(--tw-rotate-x) var(--tw-rotate-y) var(--tw-rotate-z) var(--tw-skew-x) var(--tw-skew-y)}
.flex-col{flex-direction:column}.place-items-center{place-items:center}.content-center{align-content:center}.items-center{align-items:center}.justify-between{justify-content:space-between}.justify-center{justify-content:center}.justify-end{justify-content:flex-end}.gap-1{gap:calc(var(--spacing) * 1)}.gap-2{gap:calc(var(--spacing) * 2)}.gap-6{gap:calc(var(--spacing) * 6)}.rounded-lg{border-radius:var(--radius-lg)}.rounded-sm{border-radius:var(--radius-sm)}.rounded-br-md{border-bottom-right-radius:var(--radius-md)}.rounded-bl-md{border-bottom-left-radius:var(--radius-md)}.border-4{border-width:4px}.border-\[1px\]{border-width:1px}.border-r-2{border-right-style:var(--tw-border-style);border-right-width:2px}.border-b-2{border-bottom-style:var(--tw-border-style);border-bottom-width:2px}.border-l-2{border-left-style:var(--tw-border-style);border-left-width:2px}.border-\[\#008ae0\]{border-color:#008ae0}.border-\[\#057a1c\]{border-color:#057a1c}.border-\[\#c70000\]{border-color:#c70000}.border-\[\#ff7b00\]{border-color:#ff7b00}.bg-\[\#008ae0\]{background-color:#008ae0}.bg-\[\#057a1c\]{background-color:#057a1c}.bg-\[\#c70000\]{background-color:#c70000}.bg-\[\#ff7b00\]{background-color:#ff7b00}.bg-amber-950{background-color:var(--color-amber-950)}.px-2{padding-inline:calc(var(--spacing) * 2)}.px-4{padding-inline:calc(var(--spacing) * 4)}.py-1{padding-block:calc(var(--spacing) * 1)}.text-center{text-align:center}.text-left{text-align:left}.text-right{text-align:right}.text-xl{font-size:var(--text-xl);line-height:var(--tw-leading, var(--text-xl--line-height))}.text-\[9px\]{font-size:9px}.font-bold{--tw-font-weight:var(--font-weight-bold);font-weight:var(--font-weight-bold)}.transition-all{transition-property:all;transition-timing-function:var(--tw-ease,var(--default-transition-timing-function));transition-duration:var(--tw-duration, var(--default-transition-duration))}.duration-500{--tw-duration:500ms;transition-duration:.5s}.ease-in-out{--tw-ease:var(--ease-in-out);transition-timing-function:var(--ease-in-out)}to{opacity:.5}.solar-color{color:#057a1c}.grid-color{color:#c70000}.home-color{color:#ff7b00}.battery-color{color:#008ae0}*{font-family:Roboto,Noto,sans-serif}:root{--speed:4s}@keyframes flash{0%,100%{opacity:1}50%{opacity:.2}}@keyframes move-right{0%{transform:translateX(0)}100%{transform:translateX(35px)}}@keyframes move-left{0%{transform:translateX(35px)}100%{transform:translateX(0)}}@keyframes move-down{0%{transform:translateY(0)}100%{transform:translateY(35px)}}@keyframes move-up{0%{transform:translateY(35px)}100%{transform:translateY(0)}}.flash{animation-name:flash;animation-duration:.5s;animation-timing-function:ease-in-out}.dot,.dot2,.dot3{width:8px;height:8px;border-radius:100%;animation-duration:var(--speed);animation-timing-function:linear;animation-iteration-count:infinite}.dot{position:absolute}.move-right{animation-name:move-right}.move-left{animation-name:move-left}.move-down{animation-name:move-down}.move-up{animation-name:move-up}@keyframes move-diagonal-1{0%{transform:translate(0,0)}50%{transform:translate(0,30px)}100%{transform:translate(20px,30px)}}@keyframes move-diagonal-2{0%{transform:translate(0,0)}50%{transform:translate(0,30px)}100%{transform:translate(-20px,30px)}}.dot2{animation-name:move-diagonal-1}.dot3{animation-name:move-diagonal-2}@media (max-width:600px){.container{width:100%}}@keyframes spin{to{transform:rotate(360deg)}}@keyframes ping{100%,75%{transform:scale(2);opacity:0}}@keyframes pulse{50%{opacity:.5}}@keyframes bounce{0%,100%{transform:translateY(-25%);animation-timing-function:cubic-bezier(0.8,0,1,1)}50%{transform:none;animation-timing-function:cubic-bezier(0,0,0.2,1)}}@property --tw-translate-x{syntax:"*";inherits:false;initial-value:0}@property --tw-translate-y{syntax:"*";inherits:false;initial-value:0}@property --tw-translate-z{syntax:"*";inherits:false;initial-value:0}@property --tw-rotate-x{syntax:"*";inherits:false;initial-value:rotateX(0)}@property --tw-rotate-y{syntax:"*";inherits:false;initial-value:rotateY(0)}@property --tw-rotate-z{syntax:"*";inherits:false;initial-value:rotateZ(0)}@property --tw-skew-x{syntax:"*";inherits:false;initial-value:skewX(0)}@property --tw-skew-y{syntax:"*";inherits:false;initial-value:skewY(0)}@property --tw-border-style{syntax:"*";inherits:false;initial-value:solid}@property --tw-font-weight{syntax:"*";inherits:false}@property --tw-duration{syntax:"*";inherits:false}@property --tw-ease{syntax:"*";inherits:false}
</style>
)rawliteral";

// RS485 Direction Control
void preTransmission() {
    digitalWrite(DE_RE_PIN, HIGH);
}

void postTransmission() {
    digitalWrite(DE_RE_PIN, LOW);
}

// Function to Read a Single Register
uint16_t readRegister(uint16_t regAddress) {
    uint16_t result = node.readHoldingRegisters(regAddress, 1);
    if (result == node.ku8MBSuccess) {
        return node.getResponseBuffer(0);
    } else {
        return 0;
    }
}
int16_t readSignedRegister(uint16_t regAddress) {
    int16_t result = node.readHoldingRegisters(regAddress, 1);
    if (result == node.ku8MBSuccess) {
        return node.getResponseBuffer(0);
    } else {
        return 0;
    }
}

uint32_t read32BitRegister(uint16_t regAddress) {
    uint8_t result = node.readHoldingRegisters(regAddress, 2); // Read 2 registers
    if (result == node.ku8MBSuccess) {
        uint16_t high = node.getResponseBuffer(0); // High 16-bit
        uint16_t low = node.getResponseBuffer(1);  // Low 16-bit

        uint32_t fullValue = ((uint32_t)high << 16) | low; // Combine
        return fullValue;
    } else {
        return 0;
    }
}
StaticJsonDocument<200> json;
// Read Modbus Data
void readFrequentModbusData() {
    json["bv"] = readRegister(42);  // Bat Voltage
    delay(DELAY);
    json["bc"] = readSignedRegister(43);  // Bat Current
    delay(DELAY);
    json["b"] = readSignedRegister(46); // Battery Power
    delay(DELAY);
    json["p1"] = readSignedRegister(48);  // Solar Power 1
    delay(DELAY);
    json["p2"] = readSignedRegister(47);  // Solar Power 2
    delay(DELAY);
    json["u"] = readRegister(67);  // UPS
    delay(DELAY);
    json["g"] = readSignedRegister(69);   // Grid Power
    delay(DELAY);
    json["l"] = readRegister(75);  // Home Usage
    delay(DELAY);
    json["gv"] = readRegister(30);  // Grid Voltage
    delay(DELAY);
    json["gh"] = readRegister(35);  // Grid Frequent
    delay(DELAY);
    json["p1v"] = readRegister(38);  // PV1 VOL
    delay(DELAY);
    json["p2v"] = readRegister(40);  // Pv2 vol
    delay(DELAY);
    json["p1c"] = readSignedRegister(39);  // PV1 CUR
    delay(DELAY);
    json["p2c"] = readSignedRegister(41);  // Pv2 CUR

    String jsonString;
    serializeJson(json, jsonString);
    Serial.println(jsonString);
    ws.textAll(jsonString);
}

void readLessChangeModbusData() {
    json["soc"] = readRegister(45); // SOC
    delay(DELAY);
    json["bt"] = readRegister(55);  // Bat Temperature
    delay(DELAY);
    json["it"] = readRegister(56);  // Inverter Temperature
    delay(DELAY);
    json["gb"] = readRegister(2074);  // Grid Buy Today
    delay(DELAY);
    json["gs"] = readRegister(2077);  // Grid Sell Today
    delay(DELAY);
    json["gbt"] = read32BitRegister(2075);  // Grid Buy Total
    delay(DELAY);
    json["gst"] = read32BitRegister(2078);  // Grid Sell Total
    delay(DELAY);
    json["bch"] = readRegister(2068);  // Bat Charge Today
    delay(DELAY);
    json["bd"] = readRegister(2071);  // Bat DisCharge Today
    delay(DELAY);
    json["pv"] = readRegister(2065);  // PV Today

    String jsonString;
    serializeJson(json, jsonString);
    Serial.println(jsonString);
    ws.textAll(jsonString);
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.println("Client Connected");
    }
}

void initWebSocket() {
    ws.onEvent(onWebSocketEvent);
    server.addHandler(&ws);
}

void setup() {
    Serial.begin(115200);
    pinMode(DE_RE_PIN, OUTPUT);
    digitalWrite(DE_RE_PIN, LOW);

    modbusSerial.begin(9600, SWSERIAL_8N1);
    node.begin(1, modbusSerial); // Slave ID 1
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");
    Serial.println(WiFi.localIP());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    lastShortRead = 0;
    lastLongRead = 0;
    initWebSocket();
    server.begin();
}

void loop() {
    unsigned long currentMillis = millis();
    if (currentMillis - lastShortRead >= SHORT_INTERVAL) {
        lastShortRead = currentMillis;
        readFrequentModbusData();
    }
    delay(100);
    if (currentMillis - lastLongRead >= LONG_INTERVAL) {
        lastLongRead = currentMillis;
        readLessChangeModbusData();
    }
    ws.cleanupClients();
}
