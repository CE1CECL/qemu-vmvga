 /*
  
  QEMU VMware Super Video Graphics Array 2 [SVGA-II]
  
  Copyright (c) 2007 Andrzej Zaborowski <balrog@zabor.org>
  
  Copyright (c) 2023-2024 Christopher Eric Lentocha <christopherericlentocha@gmail.com>
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
  
 */
//#define VERBOSE
#include <pthread.h>
#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "hw/loader.h"
#include "trace.h"
#include "hw/pci/pci.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "qom/object.h"
#include "vga_int.h"
#include "include/includeCheck.h"
#include "include/svga3d_caps.h"
#include "include/svga3d_cmd.h"
#include "include/svga3d_devcaps.h"
#include "include/svga3d_dx.h"
#include "include/svga3d_limits.h"
#include "include/svga3d_reg.h"
#include "include/svga3d_shaderdefs.h"
#include "include/svga3d_surfacedefs.h"
#include "include/svga3d_types.h"
#include "include/svga_escape.h"
#include "include/svga_overlay.h"
#include "include/svga_reg.h"
#include "include/svga_types.h"
#include "include/VGPU10ShaderTokens.h"
#include "include/vmware_pack_begin.h"
#include "include/vmware_pack_end.h"
#define SVGA_PIXMAP_SIZE(w, h, bpp)(((((w) * (bpp))) >> 5) * (h))
#define SVGA_CMD_RECT_FILL 2
#define SVGA_CMD_DISPLAY_CURSOR 20
#define SVGA_CMD_MOVE_CURSOR 21
#define SVGA_CAP_DX 0x10000000
#define SVGA_CAP_HP_CMD_QUEUE 0x20000000
#define SVGA_CAP_NO_BB_RESTRICTION 0x40000000
#define SVGA_CAP2_DX2 0x00000004
#define SVGA_CAP2_GB_MEMSIZE_2 0x00000008
#define SVGA_CAP2_SCREENDMA_REG 0x00000010
#define SVGA_CAP2_OTABLE_PTDEPTH_2 0x00000020
#define SVGA_CAP2_NON_MS_TO_MS_STRETCHBLT 0x00000040
#define SVGA_CAP2_CURSOR_MOB 0x00000080
#define SVGA_CAP2_MSHINT 0x00000100
#define SVGA_CAP2_CB_MAX_SIZE_4MB 0x00000200
#define SVGA_CAP2_DX3 0x00000400
#define SVGA_CAP2_FRAME_TYPE 0x00000800
#define SVGA_CAP2_COTABLE_COPY 0x00001000
#define SVGA_CAP2_TRACE_FULL_FB 0x00002000
#define SVGA_CAP2_EXTRA_REGS 0x00004000
#define SVGA_CAP2_LO_STAGING 0x00008000
#define SVGA_CAP2_VIDEO_BLT 0x00010000
#define SVGA_REG_GBOBJECT_MEM_SIZE_KB 76
#define SVGA_REG_FENCE_GOAL 84
#define SVGA_REG_MSHINT 81
#define SVGA_PALETTE_BASE_0 1024
#define SVGA_PALETTE_BASE_1 1025
#define SVGA_PALETTE_BASE_2 1026
#define SVGA_PALETTE_BASE_3 1027
#define SVGA_PALETTE_BASE_4 1028
#define SVGA_PALETTE_BASE_5 1029
#define SVGA_PALETTE_BASE_6 1030
#define SVGA_PALETTE_BASE_7 1031
#define SVGA_PALETTE_BASE_8 1032
#define SVGA_PALETTE_BASE_9 1033
#define SVGA_PALETTE_BASE_10 1034
#define SVGA_PALETTE_BASE_11 1035
#define SVGA_PALETTE_BASE_12 1036
#define SVGA_PALETTE_BASE_13 1037
#define SVGA_PALETTE_BASE_14 1038
#define SVGA_PALETTE_BASE_15 1039
#define SVGA_PALETTE_BASE_16 1040
#define SVGA_PALETTE_BASE_17 1041
#define SVGA_PALETTE_BASE_18 1042
#define SVGA_PALETTE_BASE_19 1043
#define SVGA_PALETTE_BASE_20 1044
#define SVGA_PALETTE_BASE_21 1045
#define SVGA_PALETTE_BASE_22 1046
#define SVGA_PALETTE_BASE_23 1047
#define SVGA_PALETTE_BASE_24 1048
#define SVGA_PALETTE_BASE_25 1049
#define SVGA_PALETTE_BASE_26 1050
#define SVGA_PALETTE_BASE_27 1051
#define SVGA_PALETTE_BASE_28 1052
#define SVGA_PALETTE_BASE_29 1053
#define SVGA_PALETTE_BASE_30 1054
#define SVGA_PALETTE_BASE_31 1055
#define SVGA_PALETTE_BASE_32 1056
#define SVGA_PALETTE_BASE_33 1057
#define SVGA_PALETTE_BASE_34 1058
#define SVGA_PALETTE_BASE_35 1059
#define SVGA_PALETTE_BASE_36 1060
#define SVGA_PALETTE_BASE_37 1061
#define SVGA_PALETTE_BASE_38 1062
#define SVGA_PALETTE_BASE_39 1063
#define SVGA_PALETTE_BASE_40 1064
#define SVGA_PALETTE_BASE_41 1065
#define SVGA_PALETTE_BASE_42 1066
#define SVGA_PALETTE_BASE_43 1067
#define SVGA_PALETTE_BASE_44 1068
#define SVGA_PALETTE_BASE_45 1069
#define SVGA_PALETTE_BASE_46 1070
#define SVGA_PALETTE_BASE_47 1071
#define SVGA_PALETTE_BASE_48 1072
#define SVGA_PALETTE_BASE_49 1073
#define SVGA_PALETTE_BASE_50 1074
#define SVGA_PALETTE_BASE_51 1075
#define SVGA_PALETTE_BASE_52 1076
#define SVGA_PALETTE_BASE_53 1077
#define SVGA_PALETTE_BASE_54 1078
#define SVGA_PALETTE_BASE_55 1079
#define SVGA_PALETTE_BASE_56 1080
#define SVGA_PALETTE_BASE_57 1081
#define SVGA_PALETTE_BASE_58 1082
#define SVGA_PALETTE_BASE_59 1083
#define SVGA_PALETTE_BASE_60 1084
#define SVGA_PALETTE_BASE_61 1085
#define SVGA_PALETTE_BASE_62 1086
#define SVGA_PALETTE_BASE_63 1087
#define SVGA_PALETTE_BASE_64 1088
#define SVGA_PALETTE_BASE_65 1089
#define SVGA_PALETTE_BASE_66 1090
#define SVGA_PALETTE_BASE_67 1091
#define SVGA_PALETTE_BASE_68 1092
#define SVGA_PALETTE_BASE_69 1093
#define SVGA_PALETTE_BASE_70 1094
#define SVGA_PALETTE_BASE_71 1095
#define SVGA_PALETTE_BASE_72 1096
#define SVGA_PALETTE_BASE_73 1097
#define SVGA_PALETTE_BASE_74 1098
#define SVGA_PALETTE_BASE_75 1099
#define SVGA_PALETTE_BASE_76 1100
#define SVGA_PALETTE_BASE_77 1101
#define SVGA_PALETTE_BASE_78 1102
#define SVGA_PALETTE_BASE_79 1103
#define SVGA_PALETTE_BASE_80 1104
#define SVGA_PALETTE_BASE_81 1105
#define SVGA_PALETTE_BASE_82 1106
#define SVGA_PALETTE_BASE_83 1107
#define SVGA_PALETTE_BASE_84 1108
#define SVGA_PALETTE_BASE_85 1109
#define SVGA_PALETTE_BASE_86 1110
#define SVGA_PALETTE_BASE_87 1111
#define SVGA_PALETTE_BASE_88 1112
#define SVGA_PALETTE_BASE_89 1113
#define SVGA_PALETTE_BASE_90 1114
#define SVGA_PALETTE_BASE_91 1115
#define SVGA_PALETTE_BASE_92 1116
#define SVGA_PALETTE_BASE_93 1117
#define SVGA_PALETTE_BASE_94 1118
#define SVGA_PALETTE_BASE_95 1119
#define SVGA_PALETTE_BASE_96 1120
#define SVGA_PALETTE_BASE_97 1121
#define SVGA_PALETTE_BASE_98 1122
#define SVGA_PALETTE_BASE_99 1123
#define SVGA_PALETTE_BASE_100 1124
#define SVGA_PALETTE_BASE_101 1125
#define SVGA_PALETTE_BASE_102 1126
#define SVGA_PALETTE_BASE_103 1127
#define SVGA_PALETTE_BASE_104 1128
#define SVGA_PALETTE_BASE_105 1129
#define SVGA_PALETTE_BASE_106 1130
#define SVGA_PALETTE_BASE_107 1131
#define SVGA_PALETTE_BASE_108 1132
#define SVGA_PALETTE_BASE_109 1133
#define SVGA_PALETTE_BASE_110 1134
#define SVGA_PALETTE_BASE_111 1135
#define SVGA_PALETTE_BASE_112 1136
#define SVGA_PALETTE_BASE_113 1137
#define SVGA_PALETTE_BASE_114 1138
#define SVGA_PALETTE_BASE_115 1139
#define SVGA_PALETTE_BASE_116 1140
#define SVGA_PALETTE_BASE_117 1141
#define SVGA_PALETTE_BASE_118 1142
#define SVGA_PALETTE_BASE_119 1143
#define SVGA_PALETTE_BASE_120 1144
#define SVGA_PALETTE_BASE_121 1145
#define SVGA_PALETTE_BASE_122 1146
#define SVGA_PALETTE_BASE_123 1147
#define SVGA_PALETTE_BASE_124 1148
#define SVGA_PALETTE_BASE_125 1149
#define SVGA_PALETTE_BASE_126 1150
#define SVGA_PALETTE_BASE_127 1151
#define SVGA_PALETTE_BASE_128 1152
#define SVGA_PALETTE_BASE_129 1153
#define SVGA_PALETTE_BASE_130 1154
#define SVGA_PALETTE_BASE_131 1155
#define SVGA_PALETTE_BASE_132 1156
#define SVGA_PALETTE_BASE_133 1157
#define SVGA_PALETTE_BASE_134 1158
#define SVGA_PALETTE_BASE_135 1159
#define SVGA_PALETTE_BASE_136 1160
#define SVGA_PALETTE_BASE_137 1161
#define SVGA_PALETTE_BASE_138 1162
#define SVGA_PALETTE_BASE_139 1163
#define SVGA_PALETTE_BASE_140 1164
#define SVGA_PALETTE_BASE_141 1165
#define SVGA_PALETTE_BASE_142 1166
#define SVGA_PALETTE_BASE_143 1167
#define SVGA_PALETTE_BASE_144 1168
#define SVGA_PALETTE_BASE_145 1169
#define SVGA_PALETTE_BASE_146 1170
#define SVGA_PALETTE_BASE_147 1171
#define SVGA_PALETTE_BASE_148 1172
#define SVGA_PALETTE_BASE_149 1173
#define SVGA_PALETTE_BASE_150 1174
#define SVGA_PALETTE_BASE_151 1175
#define SVGA_PALETTE_BASE_152 1176
#define SVGA_PALETTE_BASE_153 1177
#define SVGA_PALETTE_BASE_154 1178
#define SVGA_PALETTE_BASE_155 1179
#define SVGA_PALETTE_BASE_156 1180
#define SVGA_PALETTE_BASE_157 1181
#define SVGA_PALETTE_BASE_158 1182
#define SVGA_PALETTE_BASE_159 1183
#define SVGA_PALETTE_BASE_160 1184
#define SVGA_PALETTE_BASE_161 1185
#define SVGA_PALETTE_BASE_162 1186
#define SVGA_PALETTE_BASE_163 1187
#define SVGA_PALETTE_BASE_164 1188
#define SVGA_PALETTE_BASE_165 1189
#define SVGA_PALETTE_BASE_166 1190
#define SVGA_PALETTE_BASE_167 1191
#define SVGA_PALETTE_BASE_168 1192
#define SVGA_PALETTE_BASE_169 1193
#define SVGA_PALETTE_BASE_170 1194
#define SVGA_PALETTE_BASE_171 1195
#define SVGA_PALETTE_BASE_172 1196
#define SVGA_PALETTE_BASE_173 1197
#define SVGA_PALETTE_BASE_174 1198
#define SVGA_PALETTE_BASE_175 1199
#define SVGA_PALETTE_BASE_176 1200
#define SVGA_PALETTE_BASE_177 1201
#define SVGA_PALETTE_BASE_178 1202
#define SVGA_PALETTE_BASE_179 1203
#define SVGA_PALETTE_BASE_180 1204
#define SVGA_PALETTE_BASE_181 1205
#define SVGA_PALETTE_BASE_182 1206
#define SVGA_PALETTE_BASE_183 1207
#define SVGA_PALETTE_BASE_184 1208
#define SVGA_PALETTE_BASE_185 1209
#define SVGA_PALETTE_BASE_186 1210
#define SVGA_PALETTE_BASE_187 1211
#define SVGA_PALETTE_BASE_188 1212
#define SVGA_PALETTE_BASE_189 1213
#define SVGA_PALETTE_BASE_190 1214
#define SVGA_PALETTE_BASE_191 1215
#define SVGA_PALETTE_BASE_192 1216
#define SVGA_PALETTE_BASE_193 1217
#define SVGA_PALETTE_BASE_194 1218
#define SVGA_PALETTE_BASE_195 1219
#define SVGA_PALETTE_BASE_196 1220
#define SVGA_PALETTE_BASE_197 1221
#define SVGA_PALETTE_BASE_198 1222
#define SVGA_PALETTE_BASE_199 1223
#define SVGA_PALETTE_BASE_200 1224
#define SVGA_PALETTE_BASE_201 1225
#define SVGA_PALETTE_BASE_202 1226
#define SVGA_PALETTE_BASE_203 1227
#define SVGA_PALETTE_BASE_204 1228
#define SVGA_PALETTE_BASE_205 1229
#define SVGA_PALETTE_BASE_206 1230
#define SVGA_PALETTE_BASE_207 1231
#define SVGA_PALETTE_BASE_208 1232
#define SVGA_PALETTE_BASE_209 1233
#define SVGA_PALETTE_BASE_210 1234
#define SVGA_PALETTE_BASE_211 1235
#define SVGA_PALETTE_BASE_212 1236
#define SVGA_PALETTE_BASE_213 1237
#define SVGA_PALETTE_BASE_214 1238
#define SVGA_PALETTE_BASE_215 1239
#define SVGA_PALETTE_BASE_216 1240
#define SVGA_PALETTE_BASE_217 1241
#define SVGA_PALETTE_BASE_218 1242
#define SVGA_PALETTE_BASE_219 1243
#define SVGA_PALETTE_BASE_220 1244
#define SVGA_PALETTE_BASE_221 1245
#define SVGA_PALETTE_BASE_222 1246
#define SVGA_PALETTE_BASE_223 1247
#define SVGA_PALETTE_BASE_224 1248
#define SVGA_PALETTE_BASE_225 1249
#define SVGA_PALETTE_BASE_226 1250
#define SVGA_PALETTE_BASE_227 1251
#define SVGA_PALETTE_BASE_228 1252
#define SVGA_PALETTE_BASE_229 1253
#define SVGA_PALETTE_BASE_230 1254
#define SVGA_PALETTE_BASE_231 1255
#define SVGA_PALETTE_BASE_232 1256
#define SVGA_PALETTE_BASE_233 1257
#define SVGA_PALETTE_BASE_234 1258
#define SVGA_PALETTE_BASE_235 1259
#define SVGA_PALETTE_BASE_236 1260
#define SVGA_PALETTE_BASE_237 1261
#define SVGA_PALETTE_BASE_238 1262
#define SVGA_PALETTE_BASE_239 1263
#define SVGA_PALETTE_BASE_240 1264
#define SVGA_PALETTE_BASE_241 1265
#define SVGA_PALETTE_BASE_242 1266
#define SVGA_PALETTE_BASE_243 1267
#define SVGA_PALETTE_BASE_244 1268
#define SVGA_PALETTE_BASE_245 1269
#define SVGA_PALETTE_BASE_246 1270
#define SVGA_PALETTE_BASE_247 1271
#define SVGA_PALETTE_BASE_248 1272
#define SVGA_PALETTE_BASE_249 1273
#define SVGA_PALETTE_BASE_250 1274
#define SVGA_PALETTE_BASE_251 1275
#define SVGA_PALETTE_BASE_252 1276
#define SVGA_PALETTE_BASE_253 1277
#define SVGA_PALETTE_BASE_254 1278
#define SVGA_PALETTE_BASE_255 1279
#define SVGA_PALETTE_BASE_256 1280
#define SVGA_PALETTE_BASE_257 1281
#define SVGA_PALETTE_BASE_258 1282
#define SVGA_PALETTE_BASE_259 1283
#define SVGA_PALETTE_BASE_260 1284
#define SVGA_PALETTE_BASE_261 1285
#define SVGA_PALETTE_BASE_262 1286
#define SVGA_PALETTE_BASE_263 1287
#define SVGA_PALETTE_BASE_264 1288
#define SVGA_PALETTE_BASE_265 1289
#define SVGA_PALETTE_BASE_266 1290
#define SVGA_PALETTE_BASE_267 1291
#define SVGA_PALETTE_BASE_268 1292
#define SVGA_PALETTE_BASE_269 1293
#define SVGA_PALETTE_BASE_270 1294
#define SVGA_PALETTE_BASE_271 1295
#define SVGA_PALETTE_BASE_272 1296
#define SVGA_PALETTE_BASE_273 1297
#define SVGA_PALETTE_BASE_274 1298
#define SVGA_PALETTE_BASE_275 1299
#define SVGA_PALETTE_BASE_276 1300
#define SVGA_PALETTE_BASE_277 1301
#define SVGA_PALETTE_BASE_278 1302
#define SVGA_PALETTE_BASE_279 1303
#define SVGA_PALETTE_BASE_280 1304
#define SVGA_PALETTE_BASE_281 1305
#define SVGA_PALETTE_BASE_282 1306
#define SVGA_PALETTE_BASE_283 1307
#define SVGA_PALETTE_BASE_284 1308
#define SVGA_PALETTE_BASE_285 1309
#define SVGA_PALETTE_BASE_286 1310
#define SVGA_PALETTE_BASE_287 1311
#define SVGA_PALETTE_BASE_288 1312
#define SVGA_PALETTE_BASE_289 1313
#define SVGA_PALETTE_BASE_290 1314
#define SVGA_PALETTE_BASE_291 1315
#define SVGA_PALETTE_BASE_292 1316
#define SVGA_PALETTE_BASE_293 1317
#define SVGA_PALETTE_BASE_294 1318
#define SVGA_PALETTE_BASE_295 1319
#define SVGA_PALETTE_BASE_296 1320
#define SVGA_PALETTE_BASE_297 1321
#define SVGA_PALETTE_BASE_298 1322
#define SVGA_PALETTE_BASE_299 1323
#define SVGA_PALETTE_BASE_300 1324
#define SVGA_PALETTE_BASE_301 1325
#define SVGA_PALETTE_BASE_302 1326
#define SVGA_PALETTE_BASE_303 1327
#define SVGA_PALETTE_BASE_304 1328
#define SVGA_PALETTE_BASE_305 1329
#define SVGA_PALETTE_BASE_306 1330
#define SVGA_PALETTE_BASE_307 1331
#define SVGA_PALETTE_BASE_308 1332
#define SVGA_PALETTE_BASE_309 1333
#define SVGA_PALETTE_BASE_310 1334
#define SVGA_PALETTE_BASE_311 1335
#define SVGA_PALETTE_BASE_312 1336
#define SVGA_PALETTE_BASE_313 1337
#define SVGA_PALETTE_BASE_314 1338
#define SVGA_PALETTE_BASE_315 1339
#define SVGA_PALETTE_BASE_316 1340
#define SVGA_PALETTE_BASE_317 1341
#define SVGA_PALETTE_BASE_318 1342
#define SVGA_PALETTE_BASE_319 1343
#define SVGA_PALETTE_BASE_320 1344
#define SVGA_PALETTE_BASE_321 1345
#define SVGA_PALETTE_BASE_322 1346
#define SVGA_PALETTE_BASE_323 1347
#define SVGA_PALETTE_BASE_324 1348
#define SVGA_PALETTE_BASE_325 1349
#define SVGA_PALETTE_BASE_326 1350
#define SVGA_PALETTE_BASE_327 1351
#define SVGA_PALETTE_BASE_328 1352
#define SVGA_PALETTE_BASE_329 1353
#define SVGA_PALETTE_BASE_330 1354
#define SVGA_PALETTE_BASE_331 1355
#define SVGA_PALETTE_BASE_332 1356
#define SVGA_PALETTE_BASE_333 1357
#define SVGA_PALETTE_BASE_334 1358
#define SVGA_PALETTE_BASE_335 1359
#define SVGA_PALETTE_BASE_336 1360
#define SVGA_PALETTE_BASE_337 1361
#define SVGA_PALETTE_BASE_338 1362
#define SVGA_PALETTE_BASE_339 1363
#define SVGA_PALETTE_BASE_340 1364
#define SVGA_PALETTE_BASE_341 1365
#define SVGA_PALETTE_BASE_342 1366
#define SVGA_PALETTE_BASE_343 1367
#define SVGA_PALETTE_BASE_344 1368
#define SVGA_PALETTE_BASE_345 1369
#define SVGA_PALETTE_BASE_346 1370
#define SVGA_PALETTE_BASE_347 1371
#define SVGA_PALETTE_BASE_348 1372
#define SVGA_PALETTE_BASE_349 1373
#define SVGA_PALETTE_BASE_350 1374
#define SVGA_PALETTE_BASE_351 1375
#define SVGA_PALETTE_BASE_352 1376
#define SVGA_PALETTE_BASE_353 1377
#define SVGA_PALETTE_BASE_354 1378
#define SVGA_PALETTE_BASE_355 1379
#define SVGA_PALETTE_BASE_356 1380
#define SVGA_PALETTE_BASE_357 1381
#define SVGA_PALETTE_BASE_358 1382
#define SVGA_PALETTE_BASE_359 1383
#define SVGA_PALETTE_BASE_360 1384
#define SVGA_PALETTE_BASE_361 1385
#define SVGA_PALETTE_BASE_362 1386
#define SVGA_PALETTE_BASE_363 1387
#define SVGA_PALETTE_BASE_364 1388
#define SVGA_PALETTE_BASE_365 1389
#define SVGA_PALETTE_BASE_366 1390
#define SVGA_PALETTE_BASE_367 1391
#define SVGA_PALETTE_BASE_368 1392
#define SVGA_PALETTE_BASE_369 1393
#define SVGA_PALETTE_BASE_370 1394
#define SVGA_PALETTE_BASE_371 1395
#define SVGA_PALETTE_BASE_372 1396
#define SVGA_PALETTE_BASE_373 1397
#define SVGA_PALETTE_BASE_374 1398
#define SVGA_PALETTE_BASE_375 1399
#define SVGA_PALETTE_BASE_376 1400
#define SVGA_PALETTE_BASE_377 1401
#define SVGA_PALETTE_BASE_378 1402
#define SVGA_PALETTE_BASE_379 1403
#define SVGA_PALETTE_BASE_380 1404
#define SVGA_PALETTE_BASE_381 1405
#define SVGA_PALETTE_BASE_382 1406
#define SVGA_PALETTE_BASE_383 1407
#define SVGA_PALETTE_BASE_384 1408
#define SVGA_PALETTE_BASE_385 1409
#define SVGA_PALETTE_BASE_386 1410
#define SVGA_PALETTE_BASE_387 1411
#define SVGA_PALETTE_BASE_388 1412
#define SVGA_PALETTE_BASE_389 1413
#define SVGA_PALETTE_BASE_390 1414
#define SVGA_PALETTE_BASE_391 1415
#define SVGA_PALETTE_BASE_392 1416
#define SVGA_PALETTE_BASE_393 1417
#define SVGA_PALETTE_BASE_394 1418
#define SVGA_PALETTE_BASE_395 1419
#define SVGA_PALETTE_BASE_396 1420
#define SVGA_PALETTE_BASE_397 1421
#define SVGA_PALETTE_BASE_398 1422
#define SVGA_PALETTE_BASE_399 1423
#define SVGA_PALETTE_BASE_400 1424
#define SVGA_PALETTE_BASE_401 1425
#define SVGA_PALETTE_BASE_402 1426
#define SVGA_PALETTE_BASE_403 1427
#define SVGA_PALETTE_BASE_404 1428
#define SVGA_PALETTE_BASE_405 1429
#define SVGA_PALETTE_BASE_406 1430
#define SVGA_PALETTE_BASE_407 1431
#define SVGA_PALETTE_BASE_408 1432
#define SVGA_PALETTE_BASE_409 1433
#define SVGA_PALETTE_BASE_410 1434
#define SVGA_PALETTE_BASE_411 1435
#define SVGA_PALETTE_BASE_412 1436
#define SVGA_PALETTE_BASE_413 1437
#define SVGA_PALETTE_BASE_414 1438
#define SVGA_PALETTE_BASE_415 1439
#define SVGA_PALETTE_BASE_416 1440
#define SVGA_PALETTE_BASE_417 1441
#define SVGA_PALETTE_BASE_418 1442
#define SVGA_PALETTE_BASE_419 1443
#define SVGA_PALETTE_BASE_420 1444
#define SVGA_PALETTE_BASE_421 1445
#define SVGA_PALETTE_BASE_422 1446
#define SVGA_PALETTE_BASE_423 1447
#define SVGA_PALETTE_BASE_424 1448
#define SVGA_PALETTE_BASE_425 1449
#define SVGA_PALETTE_BASE_426 1450
#define SVGA_PALETTE_BASE_427 1451
#define SVGA_PALETTE_BASE_428 1452
#define SVGA_PALETTE_BASE_429 1453
#define SVGA_PALETTE_BASE_430 1454
#define SVGA_PALETTE_BASE_431 1455
#define SVGA_PALETTE_BASE_432 1456
#define SVGA_PALETTE_BASE_433 1457
#define SVGA_PALETTE_BASE_434 1458
#define SVGA_PALETTE_BASE_435 1459
#define SVGA_PALETTE_BASE_436 1460
#define SVGA_PALETTE_BASE_437 1461
#define SVGA_PALETTE_BASE_438 1462
#define SVGA_PALETTE_BASE_439 1463
#define SVGA_PALETTE_BASE_440 1464
#define SVGA_PALETTE_BASE_441 1465
#define SVGA_PALETTE_BASE_442 1466
#define SVGA_PALETTE_BASE_443 1467
#define SVGA_PALETTE_BASE_444 1468
#define SVGA_PALETTE_BASE_445 1469
#define SVGA_PALETTE_BASE_446 1470
#define SVGA_PALETTE_BASE_447 1471
#define SVGA_PALETTE_BASE_448 1472
#define SVGA_PALETTE_BASE_449 1473
#define SVGA_PALETTE_BASE_450 1474
#define SVGA_PALETTE_BASE_451 1475
#define SVGA_PALETTE_BASE_452 1476
#define SVGA_PALETTE_BASE_453 1477
#define SVGA_PALETTE_BASE_454 1478
#define SVGA_PALETTE_BASE_455 1479
#define SVGA_PALETTE_BASE_456 1480
#define SVGA_PALETTE_BASE_457 1481
#define SVGA_PALETTE_BASE_458 1482
#define SVGA_PALETTE_BASE_459 1483
#define SVGA_PALETTE_BASE_460 1484
#define SVGA_PALETTE_BASE_461 1485
#define SVGA_PALETTE_BASE_462 1486
#define SVGA_PALETTE_BASE_463 1487
#define SVGA_PALETTE_BASE_464 1488
#define SVGA_PALETTE_BASE_465 1489
#define SVGA_PALETTE_BASE_466 1490
#define SVGA_PALETTE_BASE_467 1491
#define SVGA_PALETTE_BASE_468 1492
#define SVGA_PALETTE_BASE_469 1493
#define SVGA_PALETTE_BASE_470 1494
#define SVGA_PALETTE_BASE_471 1495
#define SVGA_PALETTE_BASE_472 1496
#define SVGA_PALETTE_BASE_473 1497
#define SVGA_PALETTE_BASE_474 1498
#define SVGA_PALETTE_BASE_475 1499
#define SVGA_PALETTE_BASE_476 1500
#define SVGA_PALETTE_BASE_477 1501
#define SVGA_PALETTE_BASE_478 1502
#define SVGA_PALETTE_BASE_479 1503
#define SVGA_PALETTE_BASE_480 1504
#define SVGA_PALETTE_BASE_481 1505
#define SVGA_PALETTE_BASE_482 1506
#define SVGA_PALETTE_BASE_483 1507
#define SVGA_PALETTE_BASE_484 1508
#define SVGA_PALETTE_BASE_485 1509
#define SVGA_PALETTE_BASE_486 1510
#define SVGA_PALETTE_BASE_487 1511
#define SVGA_PALETTE_BASE_488 1512
#define SVGA_PALETTE_BASE_489 1513
#define SVGA_PALETTE_BASE_490 1514
#define SVGA_PALETTE_BASE_491 1515
#define SVGA_PALETTE_BASE_492 1516
#define SVGA_PALETTE_BASE_493 1517
#define SVGA_PALETTE_BASE_494 1518
#define SVGA_PALETTE_BASE_495 1519
#define SVGA_PALETTE_BASE_496 1520
#define SVGA_PALETTE_BASE_497 1521
#define SVGA_PALETTE_BASE_498 1522
#define SVGA_PALETTE_BASE_499 1523
#define SVGA_PALETTE_BASE_500 1524
#define SVGA_PALETTE_BASE_501 1525
#define SVGA_PALETTE_BASE_502 1526
#define SVGA_PALETTE_BASE_503 1527
#define SVGA_PALETTE_BASE_504 1528
#define SVGA_PALETTE_BASE_505 1529
#define SVGA_PALETTE_BASE_506 1530
#define SVGA_PALETTE_BASE_507 1531
#define SVGA_PALETTE_BASE_508 1532
#define SVGA_PALETTE_BASE_509 1533
#define SVGA_PALETTE_BASE_510 1534
#define SVGA_PALETTE_BASE_511 1535
#define SVGA_PALETTE_BASE_512 1536
#define SVGA_PALETTE_BASE_513 1537
#define SVGA_PALETTE_BASE_514 1538
#define SVGA_PALETTE_BASE_515 1539
#define SVGA_PALETTE_BASE_516 1540
#define SVGA_PALETTE_BASE_517 1541
#define SVGA_PALETTE_BASE_518 1542
#define SVGA_PALETTE_BASE_519 1543
#define SVGA_PALETTE_BASE_520 1544
#define SVGA_PALETTE_BASE_521 1545
#define SVGA_PALETTE_BASE_522 1546
#define SVGA_PALETTE_BASE_523 1547
#define SVGA_PALETTE_BASE_524 1548
#define SVGA_PALETTE_BASE_525 1549
#define SVGA_PALETTE_BASE_526 1550
#define SVGA_PALETTE_BASE_527 1551
#define SVGA_PALETTE_BASE_528 1552
#define SVGA_PALETTE_BASE_529 1553
#define SVGA_PALETTE_BASE_530 1554
#define SVGA_PALETTE_BASE_531 1555
#define SVGA_PALETTE_BASE_532 1556
#define SVGA_PALETTE_BASE_533 1557
#define SVGA_PALETTE_BASE_534 1558
#define SVGA_PALETTE_BASE_535 1559
#define SVGA_PALETTE_BASE_536 1560
#define SVGA_PALETTE_BASE_537 1561
#define SVGA_PALETTE_BASE_538 1562
#define SVGA_PALETTE_BASE_539 1563
#define SVGA_PALETTE_BASE_540 1564
#define SVGA_PALETTE_BASE_541 1565
#define SVGA_PALETTE_BASE_542 1566
#define SVGA_PALETTE_BASE_543 1567
#define SVGA_PALETTE_BASE_544 1568
#define SVGA_PALETTE_BASE_545 1569
#define SVGA_PALETTE_BASE_546 1570
#define SVGA_PALETTE_BASE_547 1571
#define SVGA_PALETTE_BASE_548 1572
#define SVGA_PALETTE_BASE_549 1573
#define SVGA_PALETTE_BASE_550 1574
#define SVGA_PALETTE_BASE_551 1575
#define SVGA_PALETTE_BASE_552 1576
#define SVGA_PALETTE_BASE_553 1577
#define SVGA_PALETTE_BASE_554 1578
#define SVGA_PALETTE_BASE_555 1579
#define SVGA_PALETTE_BASE_556 1580
#define SVGA_PALETTE_BASE_557 1581
#define SVGA_PALETTE_BASE_558 1582
#define SVGA_PALETTE_BASE_559 1583
#define SVGA_PALETTE_BASE_560 1584
#define SVGA_PALETTE_BASE_561 1585
#define SVGA_PALETTE_BASE_562 1586
#define SVGA_PALETTE_BASE_563 1587
#define SVGA_PALETTE_BASE_564 1588
#define SVGA_PALETTE_BASE_565 1589
#define SVGA_PALETTE_BASE_566 1590
#define SVGA_PALETTE_BASE_567 1591
#define SVGA_PALETTE_BASE_568 1592
#define SVGA_PALETTE_BASE_569 1593
#define SVGA_PALETTE_BASE_570 1594
#define SVGA_PALETTE_BASE_571 1595
#define SVGA_PALETTE_BASE_572 1596
#define SVGA_PALETTE_BASE_573 1597
#define SVGA_PALETTE_BASE_574 1598
#define SVGA_PALETTE_BASE_575 1599
#define SVGA_PALETTE_BASE_576 1600
#define SVGA_PALETTE_BASE_577 1601
#define SVGA_PALETTE_BASE_578 1602
#define SVGA_PALETTE_BASE_579 1603
#define SVGA_PALETTE_BASE_580 1604
#define SVGA_PALETTE_BASE_581 1605
#define SVGA_PALETTE_BASE_582 1606
#define SVGA_PALETTE_BASE_583 1607
#define SVGA_PALETTE_BASE_584 1608
#define SVGA_PALETTE_BASE_585 1609
#define SVGA_PALETTE_BASE_586 1610
#define SVGA_PALETTE_BASE_587 1611
#define SVGA_PALETTE_BASE_588 1612
#define SVGA_PALETTE_BASE_589 1613
#define SVGA_PALETTE_BASE_590 1614
#define SVGA_PALETTE_BASE_591 1615
#define SVGA_PALETTE_BASE_592 1616
#define SVGA_PALETTE_BASE_593 1617
#define SVGA_PALETTE_BASE_594 1618
#define SVGA_PALETTE_BASE_595 1619
#define SVGA_PALETTE_BASE_596 1620
#define SVGA_PALETTE_BASE_597 1621
#define SVGA_PALETTE_BASE_598 1622
#define SVGA_PALETTE_BASE_599 1623
#define SVGA_PALETTE_BASE_600 1624
#define SVGA_PALETTE_BASE_601 1625
#define SVGA_PALETTE_BASE_602 1626
#define SVGA_PALETTE_BASE_603 1627
#define SVGA_PALETTE_BASE_604 1628
#define SVGA_PALETTE_BASE_605 1629
#define SVGA_PALETTE_BASE_606 1630
#define SVGA_PALETTE_BASE_607 1631
#define SVGA_PALETTE_BASE_608 1632
#define SVGA_PALETTE_BASE_609 1633
#define SVGA_PALETTE_BASE_610 1634
#define SVGA_PALETTE_BASE_611 1635
#define SVGA_PALETTE_BASE_612 1636
#define SVGA_PALETTE_BASE_613 1637
#define SVGA_PALETTE_BASE_614 1638
#define SVGA_PALETTE_BASE_615 1639
#define SVGA_PALETTE_BASE_616 1640
#define SVGA_PALETTE_BASE_617 1641
#define SVGA_PALETTE_BASE_618 1642
#define SVGA_PALETTE_BASE_619 1643
#define SVGA_PALETTE_BASE_620 1644
#define SVGA_PALETTE_BASE_621 1645
#define SVGA_PALETTE_BASE_622 1646
#define SVGA_PALETTE_BASE_623 1647
#define SVGA_PALETTE_BASE_624 1648
#define SVGA_PALETTE_BASE_625 1649
#define SVGA_PALETTE_BASE_626 1650
#define SVGA_PALETTE_BASE_627 1651
#define SVGA_PALETTE_BASE_628 1652
#define SVGA_PALETTE_BASE_629 1653
#define SVGA_PALETTE_BASE_630 1654
#define SVGA_PALETTE_BASE_631 1655
#define SVGA_PALETTE_BASE_632 1656
#define SVGA_PALETTE_BASE_633 1657
#define SVGA_PALETTE_BASE_634 1658
#define SVGA_PALETTE_BASE_635 1659
#define SVGA_PALETTE_BASE_636 1660
#define SVGA_PALETTE_BASE_637 1661
#define SVGA_PALETTE_BASE_638 1662
#define SVGA_PALETTE_BASE_639 1663
#define SVGA_PALETTE_BASE_640 1664
#define SVGA_PALETTE_BASE_641 1665
#define SVGA_PALETTE_BASE_642 1666
#define SVGA_PALETTE_BASE_643 1667
#define SVGA_PALETTE_BASE_644 1668
#define SVGA_PALETTE_BASE_645 1669
#define SVGA_PALETTE_BASE_646 1670
#define SVGA_PALETTE_BASE_647 1671
#define SVGA_PALETTE_BASE_648 1672
#define SVGA_PALETTE_BASE_649 1673
#define SVGA_PALETTE_BASE_650 1674
#define SVGA_PALETTE_BASE_651 1675
#define SVGA_PALETTE_BASE_652 1676
#define SVGA_PALETTE_BASE_653 1677
#define SVGA_PALETTE_BASE_654 1678
#define SVGA_PALETTE_BASE_655 1679
#define SVGA_PALETTE_BASE_656 1680
#define SVGA_PALETTE_BASE_657 1681
#define SVGA_PALETTE_BASE_658 1682
#define SVGA_PALETTE_BASE_659 1683
#define SVGA_PALETTE_BASE_660 1684
#define SVGA_PALETTE_BASE_661 1685
#define SVGA_PALETTE_BASE_662 1686
#define SVGA_PALETTE_BASE_663 1687
#define SVGA_PALETTE_BASE_664 1688
#define SVGA_PALETTE_BASE_665 1689
#define SVGA_PALETTE_BASE_666 1690
#define SVGA_PALETTE_BASE_667 1691
#define SVGA_PALETTE_BASE_668 1692
#define SVGA_PALETTE_BASE_669 1693
#define SVGA_PALETTE_BASE_670 1694
#define SVGA_PALETTE_BASE_671 1695
#define SVGA_PALETTE_BASE_672 1696
#define SVGA_PALETTE_BASE_673 1697
#define SVGA_PALETTE_BASE_674 1698
#define SVGA_PALETTE_BASE_675 1699
#define SVGA_PALETTE_BASE_676 1700
#define SVGA_PALETTE_BASE_677 1701
#define SVGA_PALETTE_BASE_678 1702
#define SVGA_PALETTE_BASE_679 1703
#define SVGA_PALETTE_BASE_680 1704
#define SVGA_PALETTE_BASE_681 1705
#define SVGA_PALETTE_BASE_682 1706
#define SVGA_PALETTE_BASE_683 1707
#define SVGA_PALETTE_BASE_684 1708
#define SVGA_PALETTE_BASE_685 1709
#define SVGA_PALETTE_BASE_686 1710
#define SVGA_PALETTE_BASE_687 1711
#define SVGA_PALETTE_BASE_688 1712
#define SVGA_PALETTE_BASE_689 1713
#define SVGA_PALETTE_BASE_690 1714
#define SVGA_PALETTE_BASE_691 1715
#define SVGA_PALETTE_BASE_692 1716
#define SVGA_PALETTE_BASE_693 1717
#define SVGA_PALETTE_BASE_694 1718
#define SVGA_PALETTE_BASE_695 1719
#define SVGA_PALETTE_BASE_696 1720
#define SVGA_PALETTE_BASE_697 1721
#define SVGA_PALETTE_BASE_698 1722
#define SVGA_PALETTE_BASE_699 1723
#define SVGA_PALETTE_BASE_700 1724
#define SVGA_PALETTE_BASE_701 1725
#define SVGA_PALETTE_BASE_702 1726
#define SVGA_PALETTE_BASE_703 1727
#define SVGA_PALETTE_BASE_704 1728
#define SVGA_PALETTE_BASE_705 1729
#define SVGA_PALETTE_BASE_706 1730
#define SVGA_PALETTE_BASE_707 1731
#define SVGA_PALETTE_BASE_708 1732
#define SVGA_PALETTE_BASE_709 1733
#define SVGA_PALETTE_BASE_710 1734
#define SVGA_PALETTE_BASE_711 1735
#define SVGA_PALETTE_BASE_712 1736
#define SVGA_PALETTE_BASE_713 1737
#define SVGA_PALETTE_BASE_714 1738
#define SVGA_PALETTE_BASE_715 1739
#define SVGA_PALETTE_BASE_716 1740
#define SVGA_PALETTE_BASE_717 1741
#define SVGA_PALETTE_BASE_718 1742
#define SVGA_PALETTE_BASE_719 1743
#define SVGA_PALETTE_BASE_720 1744
#define SVGA_PALETTE_BASE_721 1745
#define SVGA_PALETTE_BASE_722 1746
#define SVGA_PALETTE_BASE_723 1747
#define SVGA_PALETTE_BASE_724 1748
#define SVGA_PALETTE_BASE_725 1749
#define SVGA_PALETTE_BASE_726 1750
#define SVGA_PALETTE_BASE_727 1751
#define SVGA_PALETTE_BASE_728 1752
#define SVGA_PALETTE_BASE_729 1753
#define SVGA_PALETTE_BASE_730 1754
#define SVGA_PALETTE_BASE_731 1755
#define SVGA_PALETTE_BASE_732 1756
#define SVGA_PALETTE_BASE_733 1757
#define SVGA_PALETTE_BASE_734 1758
#define SVGA_PALETTE_BASE_735 1759
#define SVGA_PALETTE_BASE_736 1760
#define SVGA_PALETTE_BASE_737 1761
#define SVGA_PALETTE_BASE_738 1762
#define SVGA_PALETTE_BASE_739 1763
#define SVGA_PALETTE_BASE_740 1764
#define SVGA_PALETTE_BASE_741 1765
#define SVGA_PALETTE_BASE_742 1766
#define SVGA_PALETTE_BASE_743 1767
#define SVGA_PALETTE_BASE_744 1768
#define SVGA_PALETTE_BASE_745 1769
#define SVGA_PALETTE_BASE_746 1770
#define SVGA_PALETTE_BASE_747 1771
#define SVGA_PALETTE_BASE_748 1772
#define SVGA_PALETTE_BASE_749 1773
#define SVGA_PALETTE_BASE_750 1774
#define SVGA_PALETTE_BASE_751 1775
#define SVGA_PALETTE_BASE_752 1776
#define SVGA_PALETTE_BASE_753 1777
#define SVGA_PALETTE_BASE_754 1778
#define SVGA_PALETTE_BASE_755 1779
#define SVGA_PALETTE_BASE_756 1780
#define SVGA_PALETTE_BASE_757 1781
#define SVGA_PALETTE_BASE_758 1782
#define SVGA_PALETTE_BASE_759 1783
#define SVGA_PALETTE_BASE_760 1784
#define SVGA_PALETTE_BASE_761 1785
#define SVGA_PALETTE_BASE_762 1786
#define SVGA_PALETTE_BASE_763 1787
#define SVGA_PALETTE_BASE_764 1788
#define SVGA_PALETTE_BASE_765 1789
#define SVGA_PALETTE_BASE_766 1790
#define SVGA_PALETTE_BASE_767 1791
#define SVGA_PALETTE_BASE_768 1792
struct vmsvga_state_s {
  uint32_t svgapalettebase0;
  uint32_t svgapalettebase1;
  uint32_t svgapalettebase2;
  uint32_t svgapalettebase3;
  uint32_t svgapalettebase4;
  uint32_t svgapalettebase5;
  uint32_t svgapalettebase6;
  uint32_t svgapalettebase7;
  uint32_t svgapalettebase8;
  uint32_t svgapalettebase9;
  uint32_t svgapalettebase10;
  uint32_t svgapalettebase11;
  uint32_t svgapalettebase12;
  uint32_t svgapalettebase13;
  uint32_t svgapalettebase14;
  uint32_t svgapalettebase15;
  uint32_t svgapalettebase16;
  uint32_t svgapalettebase17;
  uint32_t svgapalettebase18;
  uint32_t svgapalettebase19;
  uint32_t svgapalettebase20;
  uint32_t svgapalettebase21;
  uint32_t svgapalettebase22;
  uint32_t svgapalettebase23;
  uint32_t svgapalettebase24;
  uint32_t svgapalettebase25;
  uint32_t svgapalettebase26;
  uint32_t svgapalettebase27;
  uint32_t svgapalettebase28;
  uint32_t svgapalettebase29;
  uint32_t svgapalettebase30;
  uint32_t svgapalettebase31;
  uint32_t svgapalettebase32;
  uint32_t svgapalettebase33;
  uint32_t svgapalettebase34;
  uint32_t svgapalettebase35;
  uint32_t svgapalettebase36;
  uint32_t svgapalettebase37;
  uint32_t svgapalettebase38;
  uint32_t svgapalettebase39;
  uint32_t svgapalettebase40;
  uint32_t svgapalettebase41;
  uint32_t svgapalettebase42;
  uint32_t svgapalettebase43;
  uint32_t svgapalettebase44;
  uint32_t svgapalettebase45;
  uint32_t svgapalettebase46;
  uint32_t svgapalettebase47;
  uint32_t svgapalettebase48;
  uint32_t svgapalettebase49;
  uint32_t svgapalettebase50;
  uint32_t svgapalettebase51;
  uint32_t svgapalettebase52;
  uint32_t svgapalettebase53;
  uint32_t svgapalettebase54;
  uint32_t svgapalettebase55;
  uint32_t svgapalettebase56;
  uint32_t svgapalettebase57;
  uint32_t svgapalettebase58;
  uint32_t svgapalettebase59;
  uint32_t svgapalettebase60;
  uint32_t svgapalettebase61;
  uint32_t svgapalettebase62;
  uint32_t svgapalettebase63;
  uint32_t svgapalettebase64;
  uint32_t svgapalettebase65;
  uint32_t svgapalettebase66;
  uint32_t svgapalettebase67;
  uint32_t svgapalettebase68;
  uint32_t svgapalettebase69;
  uint32_t svgapalettebase70;
  uint32_t svgapalettebase71;
  uint32_t svgapalettebase72;
  uint32_t svgapalettebase73;
  uint32_t svgapalettebase74;
  uint32_t svgapalettebase75;
  uint32_t svgapalettebase76;
  uint32_t svgapalettebase77;
  uint32_t svgapalettebase78;
  uint32_t svgapalettebase79;
  uint32_t svgapalettebase80;
  uint32_t svgapalettebase81;
  uint32_t svgapalettebase82;
  uint32_t svgapalettebase83;
  uint32_t svgapalettebase84;
  uint32_t svgapalettebase85;
  uint32_t svgapalettebase86;
  uint32_t svgapalettebase87;
  uint32_t svgapalettebase88;
  uint32_t svgapalettebase89;
  uint32_t svgapalettebase90;
  uint32_t svgapalettebase91;
  uint32_t svgapalettebase92;
  uint32_t svgapalettebase93;
  uint32_t svgapalettebase94;
  uint32_t svgapalettebase95;
  uint32_t svgapalettebase96;
  uint32_t svgapalettebase97;
  uint32_t svgapalettebase98;
  uint32_t svgapalettebase99;
  uint32_t svgapalettebase100;
  uint32_t svgapalettebase101;
  uint32_t svgapalettebase102;
  uint32_t svgapalettebase103;
  uint32_t svgapalettebase104;
  uint32_t svgapalettebase105;
  uint32_t svgapalettebase106;
  uint32_t svgapalettebase107;
  uint32_t svgapalettebase108;
  uint32_t svgapalettebase109;
  uint32_t svgapalettebase110;
  uint32_t svgapalettebase111;
  uint32_t svgapalettebase112;
  uint32_t svgapalettebase113;
  uint32_t svgapalettebase114;
  uint32_t svgapalettebase115;
  uint32_t svgapalettebase116;
  uint32_t svgapalettebase117;
  uint32_t svgapalettebase118;
  uint32_t svgapalettebase119;
  uint32_t svgapalettebase120;
  uint32_t svgapalettebase121;
  uint32_t svgapalettebase122;
  uint32_t svgapalettebase123;
  uint32_t svgapalettebase124;
  uint32_t svgapalettebase125;
  uint32_t svgapalettebase126;
  uint32_t svgapalettebase127;
  uint32_t svgapalettebase128;
  uint32_t svgapalettebase129;
  uint32_t svgapalettebase130;
  uint32_t svgapalettebase131;
  uint32_t svgapalettebase132;
  uint32_t svgapalettebase133;
  uint32_t svgapalettebase134;
  uint32_t svgapalettebase135;
  uint32_t svgapalettebase136;
  uint32_t svgapalettebase137;
  uint32_t svgapalettebase138;
  uint32_t svgapalettebase139;
  uint32_t svgapalettebase140;
  uint32_t svgapalettebase141;
  uint32_t svgapalettebase142;
  uint32_t svgapalettebase143;
  uint32_t svgapalettebase144;
  uint32_t svgapalettebase145;
  uint32_t svgapalettebase146;
  uint32_t svgapalettebase147;
  uint32_t svgapalettebase148;
  uint32_t svgapalettebase149;
  uint32_t svgapalettebase150;
  uint32_t svgapalettebase151;
  uint32_t svgapalettebase152;
  uint32_t svgapalettebase153;
  uint32_t svgapalettebase154;
  uint32_t svgapalettebase155;
  uint32_t svgapalettebase156;
  uint32_t svgapalettebase157;
  uint32_t svgapalettebase158;
  uint32_t svgapalettebase159;
  uint32_t svgapalettebase160;
  uint32_t svgapalettebase161;
  uint32_t svgapalettebase162;
  uint32_t svgapalettebase163;
  uint32_t svgapalettebase164;
  uint32_t svgapalettebase165;
  uint32_t svgapalettebase166;
  uint32_t svgapalettebase167;
  uint32_t svgapalettebase168;
  uint32_t svgapalettebase169;
  uint32_t svgapalettebase170;
  uint32_t svgapalettebase171;
  uint32_t svgapalettebase172;
  uint32_t svgapalettebase173;
  uint32_t svgapalettebase174;
  uint32_t svgapalettebase175;
  uint32_t svgapalettebase176;
  uint32_t svgapalettebase177;
  uint32_t svgapalettebase178;
  uint32_t svgapalettebase179;
  uint32_t svgapalettebase180;
  uint32_t svgapalettebase181;
  uint32_t svgapalettebase182;
  uint32_t svgapalettebase183;
  uint32_t svgapalettebase184;
  uint32_t svgapalettebase185;
  uint32_t svgapalettebase186;
  uint32_t svgapalettebase187;
  uint32_t svgapalettebase188;
  uint32_t svgapalettebase189;
  uint32_t svgapalettebase190;
  uint32_t svgapalettebase191;
  uint32_t svgapalettebase192;
  uint32_t svgapalettebase193;
  uint32_t svgapalettebase194;
  uint32_t svgapalettebase195;
  uint32_t svgapalettebase196;
  uint32_t svgapalettebase197;
  uint32_t svgapalettebase198;
  uint32_t svgapalettebase199;
  uint32_t svgapalettebase200;
  uint32_t svgapalettebase201;
  uint32_t svgapalettebase202;
  uint32_t svgapalettebase203;
  uint32_t svgapalettebase204;
  uint32_t svgapalettebase205;
  uint32_t svgapalettebase206;
  uint32_t svgapalettebase207;
  uint32_t svgapalettebase208;
  uint32_t svgapalettebase209;
  uint32_t svgapalettebase210;
  uint32_t svgapalettebase211;
  uint32_t svgapalettebase212;
  uint32_t svgapalettebase213;
  uint32_t svgapalettebase214;
  uint32_t svgapalettebase215;
  uint32_t svgapalettebase216;
  uint32_t svgapalettebase217;
  uint32_t svgapalettebase218;
  uint32_t svgapalettebase219;
  uint32_t svgapalettebase220;
  uint32_t svgapalettebase221;
  uint32_t svgapalettebase222;
  uint32_t svgapalettebase223;
  uint32_t svgapalettebase224;
  uint32_t svgapalettebase225;
  uint32_t svgapalettebase226;
  uint32_t svgapalettebase227;
  uint32_t svgapalettebase228;
  uint32_t svgapalettebase229;
  uint32_t svgapalettebase230;
  uint32_t svgapalettebase231;
  uint32_t svgapalettebase232;
  uint32_t svgapalettebase233;
  uint32_t svgapalettebase234;
  uint32_t svgapalettebase235;
  uint32_t svgapalettebase236;
  uint32_t svgapalettebase237;
  uint32_t svgapalettebase238;
  uint32_t svgapalettebase239;
  uint32_t svgapalettebase240;
  uint32_t svgapalettebase241;
  uint32_t svgapalettebase242;
  uint32_t svgapalettebase243;
  uint32_t svgapalettebase244;
  uint32_t svgapalettebase245;
  uint32_t svgapalettebase246;
  uint32_t svgapalettebase247;
  uint32_t svgapalettebase248;
  uint32_t svgapalettebase249;
  uint32_t svgapalettebase250;
  uint32_t svgapalettebase251;
  uint32_t svgapalettebase252;
  uint32_t svgapalettebase253;
  uint32_t svgapalettebase254;
  uint32_t svgapalettebase255;
  uint32_t svgapalettebase256;
  uint32_t svgapalettebase257;
  uint32_t svgapalettebase258;
  uint32_t svgapalettebase259;
  uint32_t svgapalettebase260;
  uint32_t svgapalettebase261;
  uint32_t svgapalettebase262;
  uint32_t svgapalettebase263;
  uint32_t svgapalettebase264;
  uint32_t svgapalettebase265;
  uint32_t svgapalettebase266;
  uint32_t svgapalettebase267;
  uint32_t svgapalettebase268;
  uint32_t svgapalettebase269;
  uint32_t svgapalettebase270;
  uint32_t svgapalettebase271;
  uint32_t svgapalettebase272;
  uint32_t svgapalettebase273;
  uint32_t svgapalettebase274;
  uint32_t svgapalettebase275;
  uint32_t svgapalettebase276;
  uint32_t svgapalettebase277;
  uint32_t svgapalettebase278;
  uint32_t svgapalettebase279;
  uint32_t svgapalettebase280;
  uint32_t svgapalettebase281;
  uint32_t svgapalettebase282;
  uint32_t svgapalettebase283;
  uint32_t svgapalettebase284;
  uint32_t svgapalettebase285;
  uint32_t svgapalettebase286;
  uint32_t svgapalettebase287;
  uint32_t svgapalettebase288;
  uint32_t svgapalettebase289;
  uint32_t svgapalettebase290;
  uint32_t svgapalettebase291;
  uint32_t svgapalettebase292;
  uint32_t svgapalettebase293;
  uint32_t svgapalettebase294;
  uint32_t svgapalettebase295;
  uint32_t svgapalettebase296;
  uint32_t svgapalettebase297;
  uint32_t svgapalettebase298;
  uint32_t svgapalettebase299;
  uint32_t svgapalettebase300;
  uint32_t svgapalettebase301;
  uint32_t svgapalettebase302;
  uint32_t svgapalettebase303;
  uint32_t svgapalettebase304;
  uint32_t svgapalettebase305;
  uint32_t svgapalettebase306;
  uint32_t svgapalettebase307;
  uint32_t svgapalettebase308;
  uint32_t svgapalettebase309;
  uint32_t svgapalettebase310;
  uint32_t svgapalettebase311;
  uint32_t svgapalettebase312;
  uint32_t svgapalettebase313;
  uint32_t svgapalettebase314;
  uint32_t svgapalettebase315;
  uint32_t svgapalettebase316;
  uint32_t svgapalettebase317;
  uint32_t svgapalettebase318;
  uint32_t svgapalettebase319;
  uint32_t svgapalettebase320;
  uint32_t svgapalettebase321;
  uint32_t svgapalettebase322;
  uint32_t svgapalettebase323;
  uint32_t svgapalettebase324;
  uint32_t svgapalettebase325;
  uint32_t svgapalettebase326;
  uint32_t svgapalettebase327;
  uint32_t svgapalettebase328;
  uint32_t svgapalettebase329;
  uint32_t svgapalettebase330;
  uint32_t svgapalettebase331;
  uint32_t svgapalettebase332;
  uint32_t svgapalettebase333;
  uint32_t svgapalettebase334;
  uint32_t svgapalettebase335;
  uint32_t svgapalettebase336;
  uint32_t svgapalettebase337;
  uint32_t svgapalettebase338;
  uint32_t svgapalettebase339;
  uint32_t svgapalettebase340;
  uint32_t svgapalettebase341;
  uint32_t svgapalettebase342;
  uint32_t svgapalettebase343;
  uint32_t svgapalettebase344;
  uint32_t svgapalettebase345;
  uint32_t svgapalettebase346;
  uint32_t svgapalettebase347;
  uint32_t svgapalettebase348;
  uint32_t svgapalettebase349;
  uint32_t svgapalettebase350;
  uint32_t svgapalettebase351;
  uint32_t svgapalettebase352;
  uint32_t svgapalettebase353;
  uint32_t svgapalettebase354;
  uint32_t svgapalettebase355;
  uint32_t svgapalettebase356;
  uint32_t svgapalettebase357;
  uint32_t svgapalettebase358;
  uint32_t svgapalettebase359;
  uint32_t svgapalettebase360;
  uint32_t svgapalettebase361;
  uint32_t svgapalettebase362;
  uint32_t svgapalettebase363;
  uint32_t svgapalettebase364;
  uint32_t svgapalettebase365;
  uint32_t svgapalettebase366;
  uint32_t svgapalettebase367;
  uint32_t svgapalettebase368;
  uint32_t svgapalettebase369;
  uint32_t svgapalettebase370;
  uint32_t svgapalettebase371;
  uint32_t svgapalettebase372;
  uint32_t svgapalettebase373;
  uint32_t svgapalettebase374;
  uint32_t svgapalettebase375;
  uint32_t svgapalettebase376;
  uint32_t svgapalettebase377;
  uint32_t svgapalettebase378;
  uint32_t svgapalettebase379;
  uint32_t svgapalettebase380;
  uint32_t svgapalettebase381;
  uint32_t svgapalettebase382;
  uint32_t svgapalettebase383;
  uint32_t svgapalettebase384;
  uint32_t svgapalettebase385;
  uint32_t svgapalettebase386;
  uint32_t svgapalettebase387;
  uint32_t svgapalettebase388;
  uint32_t svgapalettebase389;
  uint32_t svgapalettebase390;
  uint32_t svgapalettebase391;
  uint32_t svgapalettebase392;
  uint32_t svgapalettebase393;
  uint32_t svgapalettebase394;
  uint32_t svgapalettebase395;
  uint32_t svgapalettebase396;
  uint32_t svgapalettebase397;
  uint32_t svgapalettebase398;
  uint32_t svgapalettebase399;
  uint32_t svgapalettebase400;
  uint32_t svgapalettebase401;
  uint32_t svgapalettebase402;
  uint32_t svgapalettebase403;
  uint32_t svgapalettebase404;
  uint32_t svgapalettebase405;
  uint32_t svgapalettebase406;
  uint32_t svgapalettebase407;
  uint32_t svgapalettebase408;
  uint32_t svgapalettebase409;
  uint32_t svgapalettebase410;
  uint32_t svgapalettebase411;
  uint32_t svgapalettebase412;
  uint32_t svgapalettebase413;
  uint32_t svgapalettebase414;
  uint32_t svgapalettebase415;
  uint32_t svgapalettebase416;
  uint32_t svgapalettebase417;
  uint32_t svgapalettebase418;
  uint32_t svgapalettebase419;
  uint32_t svgapalettebase420;
  uint32_t svgapalettebase421;
  uint32_t svgapalettebase422;
  uint32_t svgapalettebase423;
  uint32_t svgapalettebase424;
  uint32_t svgapalettebase425;
  uint32_t svgapalettebase426;
  uint32_t svgapalettebase427;
  uint32_t svgapalettebase428;
  uint32_t svgapalettebase429;
  uint32_t svgapalettebase430;
  uint32_t svgapalettebase431;
  uint32_t svgapalettebase432;
  uint32_t svgapalettebase433;
  uint32_t svgapalettebase434;
  uint32_t svgapalettebase435;
  uint32_t svgapalettebase436;
  uint32_t svgapalettebase437;
  uint32_t svgapalettebase438;
  uint32_t svgapalettebase439;
  uint32_t svgapalettebase440;
  uint32_t svgapalettebase441;
  uint32_t svgapalettebase442;
  uint32_t svgapalettebase443;
  uint32_t svgapalettebase444;
  uint32_t svgapalettebase445;
  uint32_t svgapalettebase446;
  uint32_t svgapalettebase447;
  uint32_t svgapalettebase448;
  uint32_t svgapalettebase449;
  uint32_t svgapalettebase450;
  uint32_t svgapalettebase451;
  uint32_t svgapalettebase452;
  uint32_t svgapalettebase453;
  uint32_t svgapalettebase454;
  uint32_t svgapalettebase455;
  uint32_t svgapalettebase456;
  uint32_t svgapalettebase457;
  uint32_t svgapalettebase458;
  uint32_t svgapalettebase459;
  uint32_t svgapalettebase460;
  uint32_t svgapalettebase461;
  uint32_t svgapalettebase462;
  uint32_t svgapalettebase463;
  uint32_t svgapalettebase464;
  uint32_t svgapalettebase465;
  uint32_t svgapalettebase466;
  uint32_t svgapalettebase467;
  uint32_t svgapalettebase468;
  uint32_t svgapalettebase469;
  uint32_t svgapalettebase470;
  uint32_t svgapalettebase471;
  uint32_t svgapalettebase472;
  uint32_t svgapalettebase473;
  uint32_t svgapalettebase474;
  uint32_t svgapalettebase475;
  uint32_t svgapalettebase476;
  uint32_t svgapalettebase477;
  uint32_t svgapalettebase478;
  uint32_t svgapalettebase479;
  uint32_t svgapalettebase480;
  uint32_t svgapalettebase481;
  uint32_t svgapalettebase482;
  uint32_t svgapalettebase483;
  uint32_t svgapalettebase484;
  uint32_t svgapalettebase485;
  uint32_t svgapalettebase486;
  uint32_t svgapalettebase487;
  uint32_t svgapalettebase488;
  uint32_t svgapalettebase489;
  uint32_t svgapalettebase490;
  uint32_t svgapalettebase491;
  uint32_t svgapalettebase492;
  uint32_t svgapalettebase493;
  uint32_t svgapalettebase494;
  uint32_t svgapalettebase495;
  uint32_t svgapalettebase496;
  uint32_t svgapalettebase497;
  uint32_t svgapalettebase498;
  uint32_t svgapalettebase499;
  uint32_t svgapalettebase500;
  uint32_t svgapalettebase501;
  uint32_t svgapalettebase502;
  uint32_t svgapalettebase503;
  uint32_t svgapalettebase504;
  uint32_t svgapalettebase505;
  uint32_t svgapalettebase506;
  uint32_t svgapalettebase507;
  uint32_t svgapalettebase508;
  uint32_t svgapalettebase509;
  uint32_t svgapalettebase510;
  uint32_t svgapalettebase511;
  uint32_t svgapalettebase512;
  uint32_t svgapalettebase513;
  uint32_t svgapalettebase514;
  uint32_t svgapalettebase515;
  uint32_t svgapalettebase516;
  uint32_t svgapalettebase517;
  uint32_t svgapalettebase518;
  uint32_t svgapalettebase519;
  uint32_t svgapalettebase520;
  uint32_t svgapalettebase521;
  uint32_t svgapalettebase522;
  uint32_t svgapalettebase523;
  uint32_t svgapalettebase524;
  uint32_t svgapalettebase525;
  uint32_t svgapalettebase526;
  uint32_t svgapalettebase527;
  uint32_t svgapalettebase528;
  uint32_t svgapalettebase529;
  uint32_t svgapalettebase530;
  uint32_t svgapalettebase531;
  uint32_t svgapalettebase532;
  uint32_t svgapalettebase533;
  uint32_t svgapalettebase534;
  uint32_t svgapalettebase535;
  uint32_t svgapalettebase536;
  uint32_t svgapalettebase537;
  uint32_t svgapalettebase538;
  uint32_t svgapalettebase539;
  uint32_t svgapalettebase540;
  uint32_t svgapalettebase541;
  uint32_t svgapalettebase542;
  uint32_t svgapalettebase543;
  uint32_t svgapalettebase544;
  uint32_t svgapalettebase545;
  uint32_t svgapalettebase546;
  uint32_t svgapalettebase547;
  uint32_t svgapalettebase548;
  uint32_t svgapalettebase549;
  uint32_t svgapalettebase550;
  uint32_t svgapalettebase551;
  uint32_t svgapalettebase552;
  uint32_t svgapalettebase553;
  uint32_t svgapalettebase554;
  uint32_t svgapalettebase555;
  uint32_t svgapalettebase556;
  uint32_t svgapalettebase557;
  uint32_t svgapalettebase558;
  uint32_t svgapalettebase559;
  uint32_t svgapalettebase560;
  uint32_t svgapalettebase561;
  uint32_t svgapalettebase562;
  uint32_t svgapalettebase563;
  uint32_t svgapalettebase564;
  uint32_t svgapalettebase565;
  uint32_t svgapalettebase566;
  uint32_t svgapalettebase567;
  uint32_t svgapalettebase568;
  uint32_t svgapalettebase569;
  uint32_t svgapalettebase570;
  uint32_t svgapalettebase571;
  uint32_t svgapalettebase572;
  uint32_t svgapalettebase573;
  uint32_t svgapalettebase574;
  uint32_t svgapalettebase575;
  uint32_t svgapalettebase576;
  uint32_t svgapalettebase577;
  uint32_t svgapalettebase578;
  uint32_t svgapalettebase579;
  uint32_t svgapalettebase580;
  uint32_t svgapalettebase581;
  uint32_t svgapalettebase582;
  uint32_t svgapalettebase583;
  uint32_t svgapalettebase584;
  uint32_t svgapalettebase585;
  uint32_t svgapalettebase586;
  uint32_t svgapalettebase587;
  uint32_t svgapalettebase588;
  uint32_t svgapalettebase589;
  uint32_t svgapalettebase590;
  uint32_t svgapalettebase591;
  uint32_t svgapalettebase592;
  uint32_t svgapalettebase593;
  uint32_t svgapalettebase594;
  uint32_t svgapalettebase595;
  uint32_t svgapalettebase596;
  uint32_t svgapalettebase597;
  uint32_t svgapalettebase598;
  uint32_t svgapalettebase599;
  uint32_t svgapalettebase600;
  uint32_t svgapalettebase601;
  uint32_t svgapalettebase602;
  uint32_t svgapalettebase603;
  uint32_t svgapalettebase604;
  uint32_t svgapalettebase605;
  uint32_t svgapalettebase606;
  uint32_t svgapalettebase607;
  uint32_t svgapalettebase608;
  uint32_t svgapalettebase609;
  uint32_t svgapalettebase610;
  uint32_t svgapalettebase611;
  uint32_t svgapalettebase612;
  uint32_t svgapalettebase613;
  uint32_t svgapalettebase614;
  uint32_t svgapalettebase615;
  uint32_t svgapalettebase616;
  uint32_t svgapalettebase617;
  uint32_t svgapalettebase618;
  uint32_t svgapalettebase619;
  uint32_t svgapalettebase620;
  uint32_t svgapalettebase621;
  uint32_t svgapalettebase622;
  uint32_t svgapalettebase623;
  uint32_t svgapalettebase624;
  uint32_t svgapalettebase625;
  uint32_t svgapalettebase626;
  uint32_t svgapalettebase627;
  uint32_t svgapalettebase628;
  uint32_t svgapalettebase629;
  uint32_t svgapalettebase630;
  uint32_t svgapalettebase631;
  uint32_t svgapalettebase632;
  uint32_t svgapalettebase633;
  uint32_t svgapalettebase634;
  uint32_t svgapalettebase635;
  uint32_t svgapalettebase636;
  uint32_t svgapalettebase637;
  uint32_t svgapalettebase638;
  uint32_t svgapalettebase639;
  uint32_t svgapalettebase640;
  uint32_t svgapalettebase641;
  uint32_t svgapalettebase642;
  uint32_t svgapalettebase643;
  uint32_t svgapalettebase644;
  uint32_t svgapalettebase645;
  uint32_t svgapalettebase646;
  uint32_t svgapalettebase647;
  uint32_t svgapalettebase648;
  uint32_t svgapalettebase649;
  uint32_t svgapalettebase650;
  uint32_t svgapalettebase651;
  uint32_t svgapalettebase652;
  uint32_t svgapalettebase653;
  uint32_t svgapalettebase654;
  uint32_t svgapalettebase655;
  uint32_t svgapalettebase656;
  uint32_t svgapalettebase657;
  uint32_t svgapalettebase658;
  uint32_t svgapalettebase659;
  uint32_t svgapalettebase660;
  uint32_t svgapalettebase661;
  uint32_t svgapalettebase662;
  uint32_t svgapalettebase663;
  uint32_t svgapalettebase664;
  uint32_t svgapalettebase665;
  uint32_t svgapalettebase666;
  uint32_t svgapalettebase667;
  uint32_t svgapalettebase668;
  uint32_t svgapalettebase669;
  uint32_t svgapalettebase670;
  uint32_t svgapalettebase671;
  uint32_t svgapalettebase672;
  uint32_t svgapalettebase673;
  uint32_t svgapalettebase674;
  uint32_t svgapalettebase675;
  uint32_t svgapalettebase676;
  uint32_t svgapalettebase677;
  uint32_t svgapalettebase678;
  uint32_t svgapalettebase679;
  uint32_t svgapalettebase680;
  uint32_t svgapalettebase681;
  uint32_t svgapalettebase682;
  uint32_t svgapalettebase683;
  uint32_t svgapalettebase684;
  uint32_t svgapalettebase685;
  uint32_t svgapalettebase686;
  uint32_t svgapalettebase687;
  uint32_t svgapalettebase688;
  uint32_t svgapalettebase689;
  uint32_t svgapalettebase690;
  uint32_t svgapalettebase691;
  uint32_t svgapalettebase692;
  uint32_t svgapalettebase693;
  uint32_t svgapalettebase694;
  uint32_t svgapalettebase695;
  uint32_t svgapalettebase696;
  uint32_t svgapalettebase697;
  uint32_t svgapalettebase698;
  uint32_t svgapalettebase699;
  uint32_t svgapalettebase700;
  uint32_t svgapalettebase701;
  uint32_t svgapalettebase702;
  uint32_t svgapalettebase703;
  uint32_t svgapalettebase704;
  uint32_t svgapalettebase705;
  uint32_t svgapalettebase706;
  uint32_t svgapalettebase707;
  uint32_t svgapalettebase708;
  uint32_t svgapalettebase709;
  uint32_t svgapalettebase710;
  uint32_t svgapalettebase711;
  uint32_t svgapalettebase712;
  uint32_t svgapalettebase713;
  uint32_t svgapalettebase714;
  uint32_t svgapalettebase715;
  uint32_t svgapalettebase716;
  uint32_t svgapalettebase717;
  uint32_t svgapalettebase718;
  uint32_t svgapalettebase719;
  uint32_t svgapalettebase720;
  uint32_t svgapalettebase721;
  uint32_t svgapalettebase722;
  uint32_t svgapalettebase723;
  uint32_t svgapalettebase724;
  uint32_t svgapalettebase725;
  uint32_t svgapalettebase726;
  uint32_t svgapalettebase727;
  uint32_t svgapalettebase728;
  uint32_t svgapalettebase729;
  uint32_t svgapalettebase730;
  uint32_t svgapalettebase731;
  uint32_t svgapalettebase732;
  uint32_t svgapalettebase733;
  uint32_t svgapalettebase734;
  uint32_t svgapalettebase735;
  uint32_t svgapalettebase736;
  uint32_t svgapalettebase737;
  uint32_t svgapalettebase738;
  uint32_t svgapalettebase739;
  uint32_t svgapalettebase740;
  uint32_t svgapalettebase741;
  uint32_t svgapalettebase742;
  uint32_t svgapalettebase743;
  uint32_t svgapalettebase744;
  uint32_t svgapalettebase745;
  uint32_t svgapalettebase746;
  uint32_t svgapalettebase747;
  uint32_t svgapalettebase748;
  uint32_t svgapalettebase749;
  uint32_t svgapalettebase750;
  uint32_t svgapalettebase751;
  uint32_t svgapalettebase752;
  uint32_t svgapalettebase753;
  uint32_t svgapalettebase754;
  uint32_t svgapalettebase755;
  uint32_t svgapalettebase756;
  uint32_t svgapalettebase757;
  uint32_t svgapalettebase758;
  uint32_t svgapalettebase759;
  uint32_t svgapalettebase760;
  uint32_t svgapalettebase761;
  uint32_t svgapalettebase762;
  uint32_t svgapalettebase763;
  uint32_t svgapalettebase764;
  uint32_t svgapalettebase765;
  uint32_t svgapalettebase766;
  uint32_t svgapalettebase767;
  uint32_t svgapalettebase768;
  uint32_t enable;
  uint32_t config;
  uint32_t index;
  uint32_t scratch_size;
  uint32_t new_width;
  uint32_t new_height;
  uint32_t new_depth;
  uint32_t num_gd;
  uint32_t disp_prim;
  uint32_t disp_x;
  uint32_t disp_y;
  uint32_t devcap_val;
  uint32_t gmrdesc;
  uint32_t gmrid;
  uint32_t gmrpage;
  uint32_t tracez;
  uint32_t cmd_low;
  uint32_t cmd_high;
  uint32_t guest;
  uint32_t svgaid;
  uint32_t thread;
  uint32_t sync;
  uint32_t bios;
  uint32_t syncing;
  uint32_t fifo_size;
  uint32_t fifo_min;
  uint32_t fifo_max;
  uint32_t fifo_next;
  uint32_t fifo_stop;
  uint32_t irq_mask;
  uint32_t irq_status;
  uint32_t display_id;
  uint32_t pitchlock;
  uint32_t cursor;
  uint32_t *fifo;
  uint32_t *scratch;
  VGACommonState vga;
  MemoryRegion fifo_ram;
};
DECLARE_INSTANCE_CHECKER(struct pci_vmsvga_state_s, VMWARE_SVGA, "vmware-svga")
struct pci_vmsvga_state_s {
  PCIDevice parent_obj;
  struct vmsvga_state_s chip;
  MemoryRegion io_bar;
};
static void cursor_update_from_fifo(struct vmsvga_state_s * s) {
  #ifdef VERBOSE
  //printf("vmsvga: cursor_update_from_fifo was just executed\n");
  #endif
  if ((s -> fifo[SVGA_FIFO_CURSOR_ON] == SVGA_CURSOR_ON_SHOW) || (s -> fifo[SVGA_FIFO_CURSOR_ON] == SVGA_CURSOR_ON_RESTORE_TO_FB)) {
    dpy_mouse_set(s -> vga.con, s -> fifo[SVGA_FIFO_CURSOR_X], s -> fifo[SVGA_FIFO_CURSOR_Y], SVGA_CURSOR_ON_SHOW);
  } else {
    dpy_mouse_set(s -> vga.con, s -> fifo[SVGA_FIFO_CURSOR_X], s -> fifo[SVGA_FIFO_CURSOR_Y], SVGA_CURSOR_ON_HIDE);
  }
}
struct vmsvga_cursor_definition_s {
  uint32_t width;
  uint32_t height;
  uint32_t id;
  uint32_t hot_x;
  uint32_t hot_y;
  uint32_t and_mask_bpp;
  uint32_t xor_mask_bpp;
  uint32_t and_mask[4096];
  uint32_t xor_mask[4096];
};
static inline void vmsvga_cursor_define(struct vmsvga_state_s * s,
  struct vmsvga_cursor_definition_s * c) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_cursor_define was just executed\n");
  #endif
  QEMUCursor * qc;
  qc = cursor_alloc(c -> width, c -> height);
  if (qc != NULL) {
    qc -> hot_x = c -> hot_x;
    qc -> hot_y = c -> hot_y;
    if (c -> xor_mask_bpp != 1 && c -> and_mask_bpp != 1) {
      uint32_t i = 0;
      uint32_t pixels = ((c -> width) * (c -> height));
      for (i = 0; i < pixels; i++) {
        qc -> data[i] = ((c -> xor_mask[i]) + (c -> and_mask[i]));
      }
    } else {
      cursor_set_mono(qc, 0xffffff, 0x000000, (void * ) c -> xor_mask, 1, (void * ) c -> and_mask);
    }
    #ifdef VERBOSE
    cursor_print_ascii_art(qc, "vmsvga_mono");
    printf("vmsvga: vmsvga_cursor_define | xor_mask == %u : and_mask == %u\n", * c -> xor_mask, * c -> and_mask);
    #endif
    dpy_cursor_define(s -> vga.con, qc);
    cursor_put(qc);
  }
}
static inline void vmsvga_rgba_cursor_define(struct vmsvga_state_s * s,
  struct vmsvga_cursor_definition_s * c) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_rgba_cursor_define was just executed\n");
  #endif
  QEMUCursor * qc;
  qc = cursor_alloc(c -> width, c -> height);
  if (qc != NULL) {
    qc -> hot_x = c -> hot_x;
    qc -> hot_y = c -> hot_y;
    if (c -> xor_mask_bpp != 1 && c -> and_mask_bpp != 1) {
      uint32_t i = 0;
      uint32_t pixels = ((c -> width) * (c -> height));
      for (i = 0; i < pixels; i++) {
        qc -> data[i] = ((c -> xor_mask[i]) + (c -> and_mask[i]));
      }
    } else {
      cursor_set_mono(qc, 0xffffff, 0x000000, (void * ) c -> xor_mask, 1, (void * ) c -> and_mask);
    }
    #ifdef VERBOSE
    cursor_print_ascii_art(qc, "vmsvga_rgba");
    printf("vmsvga: vmsvga_rgba_cursor_define | xor_mask == %u : and_mask == %u\n", * c -> xor_mask, * c -> and_mask);
    #endif
    dpy_cursor_define(s -> vga.con, qc);
    cursor_put(qc);
  }
}
static inline int vmsvga_fifo_length(struct vmsvga_state_s * s) {
  #ifdef VERBOSE
  //printf("vmsvga: vmsvga_fifo_length was just executed\n");
  #endif
  uint32_t num;
  s -> fifo_min = le32_to_cpu(s -> fifo[SVGA_FIFO_MIN]);
  s -> fifo_max = le32_to_cpu(s -> fifo[SVGA_FIFO_MAX]);
  s -> fifo_next = le32_to_cpu(s -> fifo[SVGA_FIFO_NEXT_CMD]);
  s -> fifo_stop = le32_to_cpu(s -> fifo[SVGA_FIFO_STOP]);
  num = s -> fifo_next - s -> fifo_stop;
  if (num < 1) {
    num += s -> fifo_max - s -> fifo_min;
  }
  return num >> 2;
}
static inline uint32_t vmsvga_fifo_read_raw(struct vmsvga_state_s * s) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_fifo_read_raw was just executed\n");
  #endif
  uint32_t cmd = s -> fifo[s -> fifo_stop >> 2];
  s -> fifo_stop += 4;
  if (s -> fifo_stop >= s -> fifo_max) {
    s -> fifo_stop = s -> fifo_min;
  }
  s -> fifo[SVGA_FIFO_STOP] = cpu_to_le32(s -> fifo_stop);
  return cmd;
}
static inline uint32_t vmsvga_fifo_read(struct vmsvga_state_s * s) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_fifo_read was just executed\n");
  #endif
  return le32_to_cpu(vmsvga_fifo_read_raw(s));
}
static void vmsvga_fifo_run(struct vmsvga_state_s * s) {
  #ifdef VERBOSE
  //printf("vmsvga: vmsvga_fifo_run was just executed\n");
  #endif
  #ifdef VERBOSE
  uint32_t UnknownCommandA;
  uint32_t UnknownCommandB;
  uint32_t UnknownCommandC;
  uint32_t UnknownCommandD;
  uint32_t UnknownCommandE;
  uint32_t UnknownCommandF;
  uint32_t UnknownCommandG;
  uint32_t UnknownCommandH;
  uint32_t UnknownCommandI;
  uint32_t UnknownCommandJ;
  uint32_t UnknownCommandK;
  uint32_t UnknownCommandL;
  uint32_t UnknownCommandM;
  uint32_t UnknownCommandN;
  uint32_t UnknownCommandO;
  uint32_t UnknownCommandP;
  uint32_t UnknownCommandQ;
  uint32_t UnknownCommandR;
  uint32_t UnknownCommandS;
  uint32_t UnknownCommandT;
  uint32_t UnknownCommandU;
  uint32_t UnknownCommandV;
  uint32_t UnknownCommandW;
  uint32_t UnknownCommandX;
  uint32_t UnknownCommandY;
  uint32_t UnknownCommandZ;
  uint32_t UnknownCommandAA;
  uint32_t UnknownCommandAB;
  uint32_t UnknownCommandAC;
  uint32_t UnknownCommandAD;
  uint32_t UnknownCommandAE;
  uint32_t UnknownCommandAF;
  uint32_t UnknownCommandAG;
  uint32_t UnknownCommandAH;
  uint32_t UnknownCommandAI;
  uint32_t UnknownCommandAJ;
  uint32_t UnknownCommandAK;
  uint32_t UnknownCommandAL;
  uint32_t UnknownCommandAM;
  uint32_t UnknownCommandAN;
  uint32_t UnknownCommandAO;
  uint32_t UnknownCommandAP;
  uint32_t UnknownCommandAQ;
  uint32_t UnknownCommandAR;
  uint32_t UnknownCommandAS;
  uint32_t UnknownCommandAT;
  uint32_t UnknownCommandAU;
  uint32_t UnknownCommandAV;
  uint32_t UnknownCommandAW;
  uint32_t UnknownCommandAX;
  uint32_t UnknownCommandAY;
  uint32_t UnknownCommandAZ;
  uint32_t UnknownCommandBA;
  uint32_t UnknownCommandBB;
  uint32_t UnknownCommandBC;
  uint32_t UnknownCommandBD;
  uint32_t dx, dy, z, gmrIdCMD, offsetPages, x, y, width, height;
  #endif
  uint32_t cmd;
  uint32_t args, len, maxloop = 1024;
  uint32_t i;
  struct vmsvga_cursor_definition_s cursor;
  uint32_t cmd_start;
  uint32_t fence_arg;
  uint32_t flags, num_pages;
  len = vmsvga_fifo_length(s);
  while (len > 0 && --maxloop > 0) {
    cmd_start = s -> fifo_stop;
    cmd = vmsvga_fifo_read(s);
    #ifdef VERBOSE
    printf("%s: Unknown command %u in SVGA command FIFO\n", __func__, cmd);
    #endif
    switch (cmd) {
    case SVGA_CMD_UPDATE:
      len -= 5;
      if (len < 1) {
        goto rewind;
      };
      #ifdef VERBOSE
      x = vmsvga_fifo_read(s);
      y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      #else
      vmsvga_fifo_read(s);
      vmsvga_fifo_read(s);
      vmsvga_fifo_read(s);
      vmsvga_fifo_read(s);
      #endif
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_UPDATE command %u in SVGA command FIFO %u %u %u %u \n", __func__, cmd, x, y, width, height);
      #endif
      break;
    case SVGA_CMD_UPDATE_VERBOSE:
      len -= 6;
      if (len < 1) {
        goto rewind;
      }
      #ifdef VERBOSE
      x = vmsvga_fifo_read(s);
      y = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      z = vmsvga_fifo_read(s);
      #else
      vmsvga_fifo_read(s);
      vmsvga_fifo_read(s);
      vmsvga_fifo_read(s);
      vmsvga_fifo_read(s);
      vmsvga_fifo_read(s);
      #endif
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_UPDATE_VERBOSE command %u in SVGA command FIFO %u %u %u %u %u\n", __func__, cmd, x, y, width, height, z);
      #endif
      break;
    case SVGA_CMD_RECT_FILL:
      len -= 6;
      if (len < 1) {
        goto rewind;
      }
      #ifdef VERBOSE
      UnknownCommandAQ = vmsvga_fifo_read(s);
      UnknownCommandAR = vmsvga_fifo_read(s);
      UnknownCommandAS = vmsvga_fifo_read(s);
      UnknownCommandAT = vmsvga_fifo_read(s);
      UnknownCommandAU = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_RECT_FILL command %u in SVGA command FIFO %u %u %u %u %u \n", __func__, cmd, UnknownCommandAQ, UnknownCommandAR, UnknownCommandAS, UnknownCommandAT, UnknownCommandAU);
      #endif
      break;
    case SVGA_CMD_RECT_COPY:
      len -= 7;
      if (len < 1) {
        goto rewind;
      }
      #ifdef VERBOSE
      x = vmsvga_fifo_read(s);
      y = vmsvga_fifo_read(s);
      dx = vmsvga_fifo_read(s);
      dy = vmsvga_fifo_read(s);
      width = vmsvga_fifo_read(s);
      height = vmsvga_fifo_read(s);
      #else
      vmsvga_fifo_read(s);
      vmsvga_fifo_read(s);
      vmsvga_fifo_read(s);
      vmsvga_fifo_read(s);
      vmsvga_fifo_read(s);
      vmsvga_fifo_read(s);
      #endif
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_RECT_COPY command %u in SVGA command FIFO %u %u %u %u %u %u \n", __func__, cmd, x, y, dx, dy, width, height);
      #endif
      break;
    case SVGA_CMD_DEFINE_CURSOR:
      len -= 8;
      if (len < 1) {
        goto rewind;
      }
      cursor.id = vmsvga_fifo_read(s);
      cursor.hot_x = vmsvga_fifo_read(s);
      cursor.hot_y = vmsvga_fifo_read(s);
      cursor.width = vmsvga_fifo_read(s);
      cursor.height = vmsvga_fifo_read(s);
      cursor.and_mask_bpp = vmsvga_fifo_read(s);
      cursor.xor_mask_bpp = vmsvga_fifo_read(s);
      args = (SVGA_PIXMAP_SIZE(cursor.width, cursor.height, cursor.and_mask_bpp) + SVGA_PIXMAP_SIZE(cursor.width, cursor.height, cursor.xor_mask_bpp));
      if (cursor.width < 1 || cursor.height < 1 || cursor.and_mask_bpp < 1 || cursor.xor_mask_bpp < 1 || cursor.width > s -> new_width || cursor.height > s -> new_height || cursor.and_mask_bpp > s -> new_depth || cursor.xor_mask_bpp > s -> new_depth) {
        #ifdef VERBOSE
        printf("%s: SVGA_CMD_DEFINE_CURSOR command %u in SVGA command FIFO %u %u %u %u %u %u %u \n", __func__, cmd, cursor.id, cursor.hot_x, cursor.hot_y, cursor.width, cursor.height, cursor.and_mask_bpp, cursor.xor_mask_bpp);
        #endif
        break;
      }
      len -= args;
      if (len < 1) {
        goto rewind;
      }
      for (args = 0; args < SVGA_PIXMAP_SIZE(cursor.width, cursor.height, cursor.and_mask_bpp); args++) {
        cursor.and_mask[args] = vmsvga_fifo_read_raw(s);
        #ifdef VERBOSE
        printf("%s: cursor.and_mask[args] %u \n", __func__, cursor.and_mask[args]);
        #endif
      }
      for (args = 0; args < SVGA_PIXMAP_SIZE(cursor.width, cursor.height, cursor.xor_mask_bpp); args++) {
        cursor.xor_mask[args] = vmsvga_fifo_read_raw(s);
        #ifdef VERBOSE
        printf("%s: cursor.xor_mask[args] %u \n", __func__, cursor.xor_mask[args]);
        #endif
      }
      vmsvga_cursor_define(s, & cursor);
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_DEFINE_CURSOR command %u in SVGA command FIFO %u %u %u %u %u %u %u \n", __func__, cmd, cursor.id, cursor.hot_x, cursor.hot_y, cursor.width, cursor.height, cursor.and_mask_bpp, cursor.xor_mask_bpp);
      #endif
      break;
    case SVGA_CMD_DEFINE_ALPHA_CURSOR:
      len -= 6;
      if (len < 1) {
        goto rewind;
      }
      cursor.id = vmsvga_fifo_read(s);
      cursor.hot_x = vmsvga_fifo_read(s);
      cursor.hot_y = vmsvga_fifo_read(s);
      cursor.width = vmsvga_fifo_read(s);
      cursor.height = vmsvga_fifo_read(s);
      cursor.and_mask_bpp = 32;
      cursor.xor_mask_bpp = 32;
      args = ((cursor.width) * (cursor.height));
      if (cursor.width < 1 || cursor.height < 1 || cursor.and_mask_bpp < 1 || cursor.xor_mask_bpp < 1 || cursor.width > s -> new_width || cursor.height > s -> new_height || cursor.and_mask_bpp > s -> new_depth || cursor.xor_mask_bpp > s -> new_depth) {
        #ifdef VERBOSE
        printf("%s: SVGA_CMD_DEFINE_ALPHA_CURSOR command %u in SVGA command FIFO %u %u %u %u %u %u %u \n", __func__, cmd, cursor.id, cursor.hot_x, cursor.hot_y, cursor.width, cursor.height, cursor.and_mask_bpp, cursor.xor_mask_bpp);
        #endif
        break;
      }
      len -= args;
      if (len < 1) {
        goto rewind;
      }
      for (i = 0; i < args; i++) {
        uint32_t rgba = vmsvga_fifo_read_raw(s);
        cursor.xor_mask[i] = rgba & 0x00ffffff;
        cursor.and_mask[i] = rgba & 0xff000000;
        #ifdef VERBOSE
        printf("%s: rgba %u \n", __func__, rgba);
        #endif
      }
      vmsvga_rgba_cursor_define(s, & cursor);
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_DEFINE_ALPHA_CURSOR command %u in SVGA command FIFO %u %u %u %u %u %u %u \n", __func__, cmd, cursor.id, cursor.hot_x, cursor.hot_y, cursor.width, cursor.height, cursor.and_mask_bpp, cursor.xor_mask_bpp);
      #endif
      break;
    case SVGA_CMD_FENCE:
      len -= 2;
      if (len < 1) {
        goto rewind;
      }
      fence_arg = vmsvga_fifo_read(s);
      s -> fifo[SVGA_FIFO_FENCE] = cpu_to_le32(fence_arg);
      if ((s -> irq_mask & (SVGA_IRQFLAG_ANY_FENCE))) {
        #ifdef VERBOSE
        printf("s->irq_status |= SVGA_IRQFLAG_ANY_FENCE\n");
        #endif
        s -> irq_status |= SVGA_IRQFLAG_ANY_FENCE;
      } else if ((s -> irq_mask & SVGA_IRQFLAG_FENCE_GOAL) && (s -> fifo[SVGA_FIFO_FENCE] == s -> fifo[SVGA_FIFO_FENCE_GOAL])) {
        #ifdef VERBOSE
        printf("s->irq_status |= SVGA_IRQFLAG_FENCE_GOAL\n");
        #endif
        s -> irq_status |= SVGA_IRQFLAG_FENCE_GOAL;
      }
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_FENCE command %u in SVGA command FIFO %u %u %u %u \n", __func__, cmd, fence_arg, s -> irq_mask, s -> irq_status, cpu_to_le32(fence_arg));
      #endif
      break;
    case SVGA_CMD_DEFINE_GMR2:
      len -= 3;
      if (len < 1) {
        goto rewind;
      }
      #ifdef VERBOSE
      UnknownCommandAW = vmsvga_fifo_read(s);
      UnknownCommandAX = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_DEFINE_GMR2 command %u in SVGA command FIFO %u %u \n", __func__, cmd, UnknownCommandAW, UnknownCommandAX);
      #endif
      break;
    case SVGA_CMD_REMAP_GMR2:
      len -= 5;
      if (len < 1) {
        goto rewind;
      }
      #ifdef VERBOSE
      gmrIdCMD = vmsvga_fifo_read(s);
      #else
      vmsvga_fifo_read(s);
      #endif
      flags = vmsvga_fifo_read(s);
      #ifdef VERBOSE
      offsetPages = vmsvga_fifo_read(s);
      #else
      vmsvga_fifo_read(s);
      #endif
      num_pages = vmsvga_fifo_read(s);
      if (flags & SVGA_REMAP_GMR2_VIA_GMR) {
        args = 2;
      } else {
        args = (flags & SVGA_REMAP_GMR2_SINGLE_PPN) ? 1 : num_pages;
        if (flags & SVGA_REMAP_GMR2_PPN64)
          args *= 2;
      }
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_REMAP_GMR2 command %u in SVGA command FIFO %u %u %u %u \n", __func__, cmd, gmrIdCMD, flags, offsetPages, num_pages);
      #endif
      break;
    case SVGA_CMD_RECT_ROP_COPY:
      len -= 8;
      if (len < 1) {
        goto rewind;
      }
      #ifdef VERBOSE
      UnknownCommandAY = vmsvga_fifo_read(s);
      UnknownCommandAZ = vmsvga_fifo_read(s);
      UnknownCommandBA = vmsvga_fifo_read(s);
      UnknownCommandBB = vmsvga_fifo_read(s);
      UnknownCommandBC = vmsvga_fifo_read(s);
      UnknownCommandBD = vmsvga_fifo_read(s);
      UnknownCommandM = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_RECT_ROP_COPY command %u in SVGA command FIFO %u %u %u %u %u %u %u \n", __func__, cmd, UnknownCommandAY, UnknownCommandAZ, UnknownCommandBA, UnknownCommandBB, UnknownCommandBC, UnknownCommandBD, UnknownCommandM);
      #endif
      break;
    case SVGA_CMD_ESCAPE:
      len -= 4;
      #ifdef VERBOSE
      UnknownCommandA = vmsvga_fifo_read(s);
      UnknownCommandB = vmsvga_fifo_read(s);
      UnknownCommandAV = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_ESCAPE command %u in SVGA command FIFO %u %u %u \n", __func__, cmd, UnknownCommandA, UnknownCommandB, UnknownCommandAV);
      #endif
      break;
    case SVGA_CMD_DEFINE_SCREEN:
      len -= 10;
      #ifdef VERBOSE
      UnknownCommandD = vmsvga_fifo_read(s);
      UnknownCommandE = vmsvga_fifo_read(s);
      UnknownCommandF = vmsvga_fifo_read(s);
      UnknownCommandG = vmsvga_fifo_read(s);
      UnknownCommandH = vmsvga_fifo_read(s);
      UnknownCommandI = vmsvga_fifo_read(s);
      UnknownCommandJ = vmsvga_fifo_read(s);
      UnknownCommandK = vmsvga_fifo_read(s);
      UnknownCommandL = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_DEFINE_SCREEN command %u in SVGA command FIFO %u %u %u %u %u %u %u %u %u \n", __func__, cmd, UnknownCommandD, UnknownCommandE, UnknownCommandF, UnknownCommandG, UnknownCommandH, UnknownCommandI, UnknownCommandJ, UnknownCommandK, UnknownCommandL);
      #endif
      break;
    case SVGA_CMD_DISPLAY_CURSOR:
      len -= 3;
      #ifdef VERBOSE
      UnknownCommandC = vmsvga_fifo_read(s);
      UnknownCommandN = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_DISPLAY_CURSOR command %u in SVGA command FIFO %u %u \n", __func__, cmd, UnknownCommandC, UnknownCommandN);
      #endif
      break;
    case SVGA_CMD_DESTROY_SCREEN:
      len -= 2;
      #ifdef VERBOSE
      UnknownCommandO = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_DESTROY_SCREEN command %u in SVGA command FIFO %u \n", __func__, cmd, UnknownCommandO);
      #endif
      break;
    case SVGA_CMD_DEFINE_GMRFB:
      len -= 6;
      #ifdef VERBOSE
      UnknownCommandP = vmsvga_fifo_read(s);
      UnknownCommandQ = vmsvga_fifo_read(s);
      UnknownCommandR = vmsvga_fifo_read(s);
      UnknownCommandS = vmsvga_fifo_read(s);
      UnknownCommandT = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_DEFINE_GMRFB command %u in SVGA command FIFO %u %u %u %u %u \n", __func__, cmd, UnknownCommandP, UnknownCommandQ, UnknownCommandR, UnknownCommandS, UnknownCommandT);
      #endif
      break;
    case SVGA_CMD_BLIT_GMRFB_TO_SCREEN:
      len -= 8;
      #ifdef VERBOSE
      UnknownCommandU = vmsvga_fifo_read(s);
      UnknownCommandV = vmsvga_fifo_read(s);
      UnknownCommandW = vmsvga_fifo_read(s);
      UnknownCommandX = vmsvga_fifo_read(s);
      UnknownCommandY = vmsvga_fifo_read(s);
      UnknownCommandZ = vmsvga_fifo_read(s);
      UnknownCommandAA = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_BLIT_GMRFB_TO_SCREEN command %u in SVGA command FIFO %u %u %u %u %u %u %u \n", __func__, cmd, UnknownCommandU, UnknownCommandV, UnknownCommandW, UnknownCommandX, UnknownCommandY, UnknownCommandZ, UnknownCommandAA);
      #endif
      break;
    case SVGA_CMD_BLIT_SCREEN_TO_GMRFB:
      len -= 8;
      #ifdef VERBOSE
      UnknownCommandAB = vmsvga_fifo_read(s);
      UnknownCommandAC = vmsvga_fifo_read(s);
      UnknownCommandAD = vmsvga_fifo_read(s);
      UnknownCommandAE = vmsvga_fifo_read(s);
      UnknownCommandAF = vmsvga_fifo_read(s);
      UnknownCommandAG = vmsvga_fifo_read(s);
      UnknownCommandAH = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_BLIT_SCREEN_TO_GMRFB command %u in SVGA command FIFO %u %u %u %u %u %u %u \n", __func__, cmd, UnknownCommandAB, UnknownCommandAC, UnknownCommandAD, UnknownCommandAE, UnknownCommandAF, UnknownCommandAG, UnknownCommandAH);
      #endif
      break;
    case SVGA_CMD_ANNOTATION_FILL:
      len -= 4;
      #ifdef VERBOSE
      UnknownCommandAI = vmsvga_fifo_read(s);
      UnknownCommandAJ = vmsvga_fifo_read(s);
      UnknownCommandAK = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_ANNOTATION_FILL command %u in SVGA command FIFO %u %u %u \n", __func__, cmd, UnknownCommandAI, UnknownCommandAJ, UnknownCommandAK);
      #endif
      break;
    case SVGA_CMD_ANNOTATION_COPY:
      len -= 4;
      #ifdef VERBOSE
      UnknownCommandAL = vmsvga_fifo_read(s);
      UnknownCommandAM = vmsvga_fifo_read(s);
      UnknownCommandAN = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_ANNOTATION_COPY command %u in SVGA command FIFO %u %u %u \n", __func__, cmd, UnknownCommandAL, UnknownCommandAM, UnknownCommandAN);
      #endif
      break;
    case SVGA_CMD_MOVE_CURSOR:
      len -= 3;
      #ifdef VERBOSE
      UnknownCommandAO = vmsvga_fifo_read(s);
      UnknownCommandAP = vmsvga_fifo_read(s);
      printf("%s: SVGA_CMD_MOVE_CURSOR command %u in SVGA command FIFO %u %u \n", __func__, cmd, UnknownCommandAO, UnknownCommandAP);
      #endif
      break;
    case SVGA_CMD_INVALID_CMD:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_INVALID_CMD command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_CMD_FRONT_ROP_FILL:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_FRONT_ROP_FILL command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_CMD_DEAD:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_DEAD command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_CMD_DEAD_2:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_DEAD_2 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_CMD_NOP:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_NOP command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_CMD_NOP_ERROR:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_NOP_ERROR command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_CMD_MAX:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_CMD_MAX command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_LEGACY_BASE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_LEGACY_BASE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SURFACE_DEFINE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SURFACE_DEFINE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SURFACE_DESTROY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SURFACE_DESTROY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SURFACE_COPY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SURFACE_COPY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SURFACE_STRETCHBLT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SURFACE_STRETCHBLT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SURFACE_DMA:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SURFACE_DMA command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_CONTEXT_DEFINE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_CONTEXT_DEFINE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_CONTEXT_DESTROY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_CONTEXT_DESTROY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SETTRANSFORM:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SETTRANSFORM command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SETZRANGE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SETZRANGE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SETRENDERSTATE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SETRENDERSTATE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SETRENDERTARGET:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SETRENDERTARGET command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SETTEXTURESTATE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SETTEXTURESTATE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SETMATERIAL:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SETMATERIAL command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SETLIGHTDATA:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SETLIGHTDATA command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SETLIGHTENABLED:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SETLIGHTENABLED command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SETVIEWPORT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SETVIEWPORT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SETCLIPPLANE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SETCLIPPLANE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_CLEAR:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_CLEAR command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_PRESENT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_PRESENT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SHADER_DEFINE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SHADER_DEFINE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SHADER_DESTROY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SHADER_DESTROY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SET_SHADER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SET_SHADER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SET_SHADER_CONST:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SET_SHADER_CONST command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DRAW_PRIMITIVES:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DRAW_PRIMITIVES command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SETSCISSORRECT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SETSCISSORRECT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_BEGIN_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_BEGIN_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_END_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_END_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_WAIT_FOR_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_WAIT_FOR_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_PRESENT_READBACK:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_PRESENT_READBACK command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SURFACE_DEFINE_V2:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SURFACE_DEFINE_V2 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_GENERATE_MIPMAPS:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_GENERATE_MIPMAPS command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD4:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD4 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD5:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD5 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD6:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD6 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD7:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD7 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD8:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD8 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD9:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD9 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD10:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD10 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD11:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD11 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_ACTIVATE_SURFACE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_ACTIVATE_SURFACE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEACTIVATE_SURFACE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEACTIVATE_SURFACE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SCREEN_DMA:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SCREEN_DMA command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD1:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_VB_DX_CLEAR_RENDERTARGET_VIEW_REGION command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD2:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD2 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD12:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD12 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD13:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD13 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD14:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD14 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD15:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD15 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD16:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD16 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD17:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD17 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SET_OTABLE_BASE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SET_OTABLE_BASE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_READBACK_OTABLE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_READBACK_OTABLE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEFINE_GB_MOB:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEFINE_GB_MOB command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DESTROY_GB_MOB:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DESTROY_GB_MOB command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEAD3:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEAD3 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_UPDATE_GB_MOB_MAPPING:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_UPDATE_GB_MOB_MAPPING command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEFINE_GB_SURFACE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEFINE_GB_SURFACE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DESTROY_GB_SURFACE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DESTROY_GB_SURFACE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_BIND_GB_SURFACE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_BIND_GB_SURFACE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_COND_BIND_GB_SURFACE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_COND_BIND_GB_SURFACE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_UPDATE_GB_IMAGE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_UPDATE_GB_IMAGE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_UPDATE_GB_SURFACE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_UPDATE_GB_SURFACE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_READBACK_GB_IMAGE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_READBACK_GB_IMAGE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_READBACK_GB_SURFACE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_READBACK_GB_SURFACE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_INVALIDATE_GB_IMAGE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_INVALIDATE_GB_IMAGE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_INVALIDATE_GB_SURFACE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_INVALIDATE_GB_SURFACE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEFINE_GB_CONTEXT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEFINE_GB_CONTEXT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DESTROY_GB_CONTEXT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DESTROY_GB_CONTEXT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_BIND_GB_CONTEXT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_BIND_GB_CONTEXT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_READBACK_GB_CONTEXT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_READBACK_GB_CONTEXT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_INVALIDATE_GB_CONTEXT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_INVALIDATE_GB_CONTEXT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEFINE_GB_SHADER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEFINE_GB_SHADER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DESTROY_GB_SHADER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DESTROY_GB_SHADER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_BIND_GB_SHADER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_BIND_GB_SHADER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SET_OTABLE_BASE64:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SET_OTABLE_BASE64 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_BEGIN_GB_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_BEGIN_GB_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_END_GB_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_END_GB_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_WAIT_FOR_GB_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_WAIT_FOR_GB_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_NOP:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_NOP command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_ENABLE_GART:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_ENABLE_GART command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DISABLE_GART:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DISABLE_GART command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_MAP_MOB_INTO_GART:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_MAP_MOB_INTO_GART command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_UNMAP_GART_RANGE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_UNMAP_GART_RANGE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEFINE_GB_SCREENTARGET:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEFINE_GB_SCREENTARGET command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DESTROY_GB_SCREENTARGET:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DESTROY_GB_SCREENTARGET command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_BIND_GB_SCREENTARGET:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_BIND_GB_SCREENTARGET command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_UPDATE_GB_SCREENTARGET:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_UPDATE_GB_SCREENTARGET command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_READBACK_GB_IMAGE_PARTIAL:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_READBACK_GB_IMAGE_PARTIAL command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_INVALIDATE_GB_IMAGE_PARTIAL:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_INVALIDATE_GB_IMAGE_PARTIAL command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SET_GB_SHADERCONSTS_INLINE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SET_GB_SHADERCONSTS_INLINE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_GB_SCREEN_DMA:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_GB_SCREEN_DMA command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_BIND_GB_SURFACE_WITH_PITCH:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_BIND_GB_SURFACE_WITH_PITCH command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_GB_MOB_FENCE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_GB_MOB_FENCE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEFINE_GB_SURFACE_V2:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEFINE_GB_SURFACE_V2 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEFINE_GB_MOB64:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEFINE_GB_MOB64 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_REDEFINE_GB_MOB64:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_REDEFINE_GB_MOB64 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_NOP_ERROR:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_NOP_ERROR command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SET_VERTEX_STREAMS:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SET_VERTEX_STREAMS command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SET_VERTEX_DECLS:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SET_VERTEX_DECLS command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SET_VERTEX_DIVISORS:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SET_VERTEX_DIVISORS command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DRAW:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DRAW command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DRAW_INDEXED:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DRAW_INDEXED command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_CONTEXT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_CONTEXT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DESTROY_CONTEXT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DESTROY_CONTEXT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_BIND_CONTEXT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_BIND_CONTEXT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_READBACK_CONTEXT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_READBACK_CONTEXT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_INVALIDATE_CONTEXT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_INVALIDATE_CONTEXT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_SINGLE_CONSTANT_BUFFER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_SINGLE_CONSTANT_BUFFER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_SHADER_RESOURCES:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_SHADER_RESOURCES command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_SHADER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_SHADER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_SAMPLERS:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_SAMPLERS command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DRAW:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DRAW command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DRAW_INDEXED:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DRAW_INDEXED command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DRAW_INSTANCED:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DRAW_INSTANCED command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DRAW_INDEXED_INSTANCED:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DRAW_INDEXED_INSTANCED command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DRAW_AUTO:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DRAW_AUTO command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_INPUT_LAYOUT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_INPUT_LAYOUT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_VERTEX_BUFFERS:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_VERTEX_BUFFERS command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_INDEX_BUFFER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_INDEX_BUFFER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_TOPOLOGY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_TOPOLOGY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_RENDERTARGETS:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_RENDERTARGETS command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_BLEND_STATE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_BLEND_STATE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_DEPTHSTENCIL_STATE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_DEPTHSTENCIL_STATE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_RASTERIZER_STATE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_RASTERIZER_STATE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DESTROY_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DESTROY_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_BIND_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_BIND_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_QUERY_OFFSET:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_QUERY_OFFSET command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_BEGIN_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_BEGIN_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_END_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_END_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_READBACK_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_READBACK_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_PREDICATION:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_PREDICATION command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_SOTARGETS:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_SOTARGETS command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_VIEWPORTS:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_VIEWPORTS command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_SCISSORRECTS:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_SCISSORRECTS command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_CLEAR_RENDERTARGET_VIEW:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_CLEAR_RENDERTARGET_VIEW command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_CLEAR_DEPTHSTENCIL_VIEW:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_CLEAR_DEPTHSTENCIL_VIEW command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_PRED_COPY_REGION:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_PRED_COPY_REGION command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_PRED_COPY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_PRED_COPY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_PRESENTBLT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_PRESENTBLT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_GENMIPS:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_GENMIPS command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_UPDATE_SUBRESOURCE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_UPDATE_SUBRESOURCE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_READBACK_SUBRESOURCE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_READBACK_SUBRESOURCE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_INVALIDATE_SUBRESOURCE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_INVALIDATE_SUBRESOURCE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_SHADERRESOURCE_VIEW:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_SHADERRESOURCE_VIEW command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DESTROY_SHADERRESOURCE_VIEW:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DESTROY_SHADERRESOURCE_VIEW command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_RENDERTARGET_VIEW:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_RENDERTARGET_VIEW command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DESTROY_RENDERTARGET_VIEW:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DESTROY_RENDERTARGET_VIEW command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_VIEW:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_VIEW command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_ELEMENTLAYOUT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_ELEMENTLAYOUT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DESTROY_ELEMENTLAYOUT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DESTROY_ELEMENTLAYOUT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_BLEND_STATE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_BLEND_STATE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DESTROY_BLEND_STATE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DESTROY_BLEND_STATE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_STATE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_STATE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_STATE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DESTROY_DEPTHSTENCIL_STATE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_RASTERIZER_STATE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_RASTERIZER_STATE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DESTROY_RASTERIZER_STATE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DESTROY_RASTERIZER_STATE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_SAMPLER_STATE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_SAMPLER_STATE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DESTROY_SAMPLER_STATE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DESTROY_SAMPLER_STATE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_SHADER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_SHADER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DESTROY_SHADER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DESTROY_SHADER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_BIND_SHADER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_BIND_SHADER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DESTROY_STREAMOUTPUT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DESTROY_STREAMOUTPUT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_STREAMOUTPUT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_STREAMOUTPUT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_COTABLE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_COTABLE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_READBACK_COTABLE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_READBACK_COTABLE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_BUFFER_COPY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_BUFFER_COPY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_TRANSFER_FROM_BUFFER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_TRANSFER_FROM_BUFFER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SURFACE_COPY_AND_READBACK:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SURFACE_COPY_AND_READBACK command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_MOVE_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_MOVE_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_BIND_ALL_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_BIND_ALL_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_READBACK_ALL_QUERY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_READBACK_ALL_QUERY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_PRED_TRANSFER_FROM_BUFFER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_PRED_TRANSFER_FROM_BUFFER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_MOB_FENCE_64:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_MOB_FENCE_64 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_BIND_ALL_SHADER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_BIND_ALL_SHADER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_HINT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_HINT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_BUFFER_UPDATE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_BUFFER_UPDATE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_VS_CONSTANT_BUFFER_OFFSET:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_VS_CONSTANT_BUFFER_OFFSET command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_PS_CONSTANT_BUFFER_OFFSET:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_PS_CONSTANT_BUFFER_OFFSET command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_GS_CONSTANT_BUFFER_OFFSET:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_GS_CONSTANT_BUFFER_OFFSET command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_HS_CONSTANT_BUFFER_OFFSET:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_HS_CONSTANT_BUFFER_OFFSET command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_DS_CONSTANT_BUFFER_OFFSET:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_DS_CONSTANT_BUFFER_OFFSET command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_CS_CONSTANT_BUFFER_OFFSET:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_CS_CONSTANT_BUFFER_OFFSET command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_COND_BIND_ALL_SHADER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_COND_BIND_ALL_SHADER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SCREEN_COPY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SCREEN_COPY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_GROW_OTABLE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_GROW_OTABLE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_GROW_COTABLE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_GROW_COTABLE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_INTRA_SURFACE_COPY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_INTRA_SURFACE_COPY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEFINE_GB_SURFACE_V3:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEFINE_GB_SURFACE_V3 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_RESOLVE_COPY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_RESOLVE_COPY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_PRED_RESOLVE_COPY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_PRED_RESOLVE_COPY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_PRED_CONVERT_REGION:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_PRED_CONVERT_REGION command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_PRED_CONVERT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_PRED_CONVERT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_WHOLE_SURFACE_COPY:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_WHOLE_SURFACE_COPY command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_UA_VIEW:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_UA_VIEW command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DESTROY_UA_VIEW:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DESTROY_UA_VIEW command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_CLEAR_UA_VIEW_UINT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_CLEAR_UA_VIEW_UINT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_CLEAR_UA_VIEW_FLOAT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_CLEAR_UA_VIEW_FLOAT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_COPY_STRUCTURE_COUNT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_COPY_STRUCTURE_COUNT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_UA_VIEWS:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_UA_VIEWS command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DRAW_INDEXED_INSTANCED_INDIRECT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DRAW_INDEXED_INSTANCED_INDIRECT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DRAW_INSTANCED_INDIRECT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DRAW_INSTANCED_INDIRECT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DISPATCH:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DISPATCH command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DISPATCH_INDIRECT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DISPATCH_INDIRECT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_WRITE_ZERO_SURFACE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_WRITE_ZERO_SURFACE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_HINT_ZERO_SURFACE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_HINT_ZERO_SURFACE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_TRANSFER_TO_BUFFER:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_TRANSFER_TO_BUFFER command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_STRUCTURE_COUNT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_STRUCTURE_COUNT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_LOGICOPS_BITBLT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_LOGICOPS_BITBLT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_LOGICOPS_TRANSBLT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_LOGICOPS_TRANSBLT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_LOGICOPS_STRETCHBLT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_LOGICOPS_STRETCHBLT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_LOGICOPS_COLORFILL:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_LOGICOPS_COLORFILL command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_LOGICOPS_ALPHABLEND:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_LOGICOPS_ALPHABLEND command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_LOGICOPS_CLEARTYPEBLEND:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_LOGICOPS_CLEARTYPEBLEND command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DEFINE_GB_SURFACE_V4:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DEFINE_GB_SURFACE_V4 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_CS_UA_VIEWS:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_CS_UA_VIEWS command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_MIN_LOD:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_MIN_LOD command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW_V2:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW_V2 command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT_WITH_MOB:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_DEFINE_STREAMOUTPUT_WITH_MOB command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_SET_SHADER_IFACE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_SET_SHADER_IFACE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_BIND_STREAMOUTPUT:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_BIND_STREAMOUTPUT command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_SURFACE_STRETCHBLT_NON_MS_TO_MS:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_SURFACE_STRETCHBLT_NON_MS_TO_MS command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_DX_BIND_SHADER_IFACE:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_DX_BIND_SHADER_IFACE command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_MAX:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_MAX command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    case SVGA_3D_CMD_FUTURE_MAX:
      len -= 1;
      #ifdef VERBOSE
      printf("%s: SVGA_3D_CMD_FUTURE_MAX command %u in SVGA command FIFO \n", __func__, cmd);
      #endif
      break;
    default:
      args = 0;
      if (len < 1) {
        goto rewind;
      }
      while (args--) {
        vmsvga_fifo_read(s);
      }
      #ifdef VERBOSE
      printf("%s: default command %u in SVGA command FIFO\n", __func__, cmd);
      #endif
      break;
      rewind:
        s -> fifo_stop = cmd_start;
      s -> fifo[SVGA_FIFO_STOP] = cpu_to_le32(s -> fifo_stop);
      #ifdef VERBOSE
      printf("%s: rewind command in SVGA command FIFO\n", __func__);
      #endif
      break;
    }
  }
  if ((s -> irq_mask & (SVGA_IRQFLAG_FIFO_PROGRESS))) {
    #ifdef VERBOSE
    printf("s->irq_status |= SVGA_IRQFLAG_FIFO_PROGRESS\n");
    #endif
    s -> irq_status |= SVGA_IRQFLAG_FIFO_PROGRESS;
  }
  struct pci_vmsvga_state_s * pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
  if (((s -> irq_mask & s -> irq_status))) {
    #ifdef VERBOSE
    printf("Pci_set_irq=1\n");
    #endif
    pci_set_irq(PCI_DEVICE(pci_vmsvga), 1);
  } else {
    #ifdef VERBOSE
    //printf("Pci_set_irq=0\n");
    #endif
    pci_set_irq(PCI_DEVICE(pci_vmsvga), 0);
  }
}
static uint32_t vmsvga_index_read(void * opaque, uint32_t address) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_index_read was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  #ifdef VERBOSE
  printf("%s: vmsvga_index_read\n", __func__);
  #endif
  return s -> index;
}
static void vmsvga_index_write(void * opaque, uint32_t address, uint32_t index) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_index_write was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  #ifdef VERBOSE
  printf("%s: vmsvga_index_write\n", __func__);
  #endif
  s -> index = index;
}
void * vmsvga_loop(void * arg);
void * vmsvga_loop(void * arg) {
  #ifdef VERBOSE
  //printf("vmsvga: vmsvga_loop was just executed\n");
  #endif
  struct vmsvga_state_s * s = (struct vmsvga_state_s * ) arg;
  uint32_t cx = 0;
  uint32_t cy = 0;
  uint32_t fc = 0;
  while (true) {
    s -> fifo[32] = 0x00000001;
    s -> fifo[33] = 0x00000008;
    s -> fifo[34] = 0x00000008;
    s -> fifo[35] = 0x00000008;
    s -> fifo[36] = 0x00000007;
    s -> fifo[37] = 0x00000001;
    s -> fifo[38] = 0x0000000d;
    s -> fifo[39] = 0x00000001;
    s -> fifo[40] = 0x00000008;
    s -> fifo[41] = 0x00000001;
    s -> fifo[42] = 0x00000001;
    s -> fifo[43] = 0x00000004;
    s -> fifo[44] = 0x00000001;
    s -> fifo[45] = 0x00000001;
    s -> fifo[46] = 0x00000001;
    s -> fifo[47] = 0x00000001;
    s -> fifo[48] = 0x00000001;
    s -> fifo[49] = 0x000000bd;
    s -> fifo[50] = 0x00000014;
    s -> fifo[51] = 0x00008000;
    s -> fifo[52] = 0x00008000;
    s -> fifo[53] = 0x00004000;
    s -> fifo[54] = 0x00008000;
    s -> fifo[55] = 0x00008000;
    s -> fifo[56] = 0x00000010;
    s -> fifo[57] = 0x001fffff;
    s -> fifo[58] = 0x000fffff;
    s -> fifo[59] = 0x0000ffff;
    s -> fifo[60] = 0x0000ffff;
    s -> fifo[61] = 0x00000020;
    s -> fifo[62] = 0x00000020;
    s -> fifo[63] = 0x03ffffff;
    s -> fifo[64] = 0x0018ec1f;
    s -> fifo[65] = 0x0018e11f;
    s -> fifo[66] = 0x0008601f;
    s -> fifo[67] = 0x0008601f;
    s -> fifo[68] = 0x0008611f;
    s -> fifo[69] = 0x0000611f;
    s -> fifo[70] = 0x0018ec1f;
    s -> fifo[71] = 0x0000601f;
    s -> fifo[72] = 0x00006007;
    s -> fifo[73] = 0x0000601f;
    s -> fifo[74] = 0x0000601f;
    s -> fifo[75] = 0x000040c5;
    s -> fifo[76] = 0x000040c5;
    s -> fifo[77] = 0x000040c5;
    s -> fifo[78] = 0x0000e005;
    s -> fifo[79] = 0x0000e005;
    s -> fifo[80] = 0x0000e005;
    s -> fifo[81] = 0x0000e005;
    s -> fifo[82] = 0x0000e005;
    s -> fifo[83] = 0x00014005;
    s -> fifo[84] = 0x00014007;
    s -> fifo[85] = 0x00014007;
    s -> fifo[86] = 0x00014005;
    s -> fifo[87] = 0x00014001;
    s -> fifo[88] = 0x0080601f;
    s -> fifo[89] = 0x0080601f;
    s -> fifo[90] = 0x0080601f;
    s -> fifo[91] = 0x0080601f;
    s -> fifo[92] = 0x0080601f;
    s -> fifo[93] = 0x0080601f;
    s -> fifo[94] = 0x00000000;
    s -> fifo[95] = 0x00000004;
    s -> fifo[96] = 0x00000008;
    s -> fifo[97] = 0x00014007;
    s -> fifo[98] = 0x0000601f;
    s -> fifo[99] = 0x0000601f;
    s -> fifo[100] = 0x01246000;
    s -> fifo[101] = 0x01246000;
    s -> fifo[102] = 0x00000000;
    s -> fifo[103] = 0x00000000;
    s -> fifo[104] = 0x00000000;
    s -> fifo[105] = 0x00000000;
    s -> fifo[106] = 0x00000001;
    s -> fifo[107] = 0x01246000;
    s -> fifo[108] = 0x00000000;
    s -> fifo[109] = 0x00000100;
    s -> fifo[110] = 0x00008000;
    s -> fifo[111] = 0x000040c5;
    s -> fifo[112] = 0x000040c5;
    s -> fifo[113] = 0x000040c5;
    s -> fifo[114] = 0x00006005;
    s -> fifo[115] = 0x00006005;
    s -> fifo[116] = 0x00000000;
    s -> fifo[117] = 0x00000000;
    s -> fifo[118] = 0x00000000;
    s -> fifo[119] = 0x00000001;
    s -> fifo[120] = 0x00000001;
    s -> fifo[121] = 0x0000000a;
    s -> fifo[122] = 0x0000000a;
    s -> fifo[123] = 0x01246000;
    s -> fifo[124] = 0x00000000;
    s -> fifo[125] = 0x00000001;
    s -> fifo[126] = 0x00000000;
    s -> fifo[127] = 0x00000001;
    s -> fifo[128] = 0x00000000;
    s -> fifo[129] = 0x00000010;
    s -> fifo[130] = 0x0000000f;
    s -> fifo[131] = 0x00000001;
    s -> fifo[132] = 0x000002f7;
    s -> fifo[133] = 0x000003f7;
    s -> fifo[134] = 0x000002f7;
    s -> fifo[135] = 0x000000f7;
    s -> fifo[136] = 0x000000f7;
    s -> fifo[137] = 0x000000f7;
    s -> fifo[138] = 0x00000009;
    s -> fifo[139] = 0x0000026b;
    s -> fifo[140] = 0x0000026b;
    s -> fifo[141] = 0x0000000b;
    s -> fifo[142] = 0x000000f7;
    s -> fifo[143] = 0x000000e3;
    s -> fifo[144] = 0x000000f7;
    s -> fifo[145] = 0x000000e3;
    s -> fifo[146] = 0x00000063;
    s -> fifo[147] = 0x00000063;
    s -> fifo[148] = 0x00000063;
    s -> fifo[149] = 0x00000063;
    s -> fifo[150] = 0x00000063;
    s -> fifo[151] = 0x000000e3;
    s -> fifo[152] = 0x00000000;
    s -> fifo[153] = 0x00000063;
    s -> fifo[154] = 0x00000000;
    s -> fifo[155] = 0x000003f7;
    s -> fifo[156] = 0x000003f7;
    s -> fifo[157] = 0x000003f7;
    s -> fifo[158] = 0x000000e3;
    s -> fifo[159] = 0x00000063;
    s -> fifo[160] = 0x00000063;
    s -> fifo[161] = 0x000000e3;
    s -> fifo[162] = 0x000000e3;
    s -> fifo[163] = 0x000000f7;
    s -> fifo[164] = 0x000003f7;
    s -> fifo[165] = 0x000003f7;
    s -> fifo[166] = 0x000003f7;
    s -> fifo[167] = 0x000003f7;
    s -> fifo[168] = 0x00000001;
    s -> fifo[169] = 0x0000026b;
    s -> fifo[170] = 0x000001e3;
    s -> fifo[171] = 0x000003f7;
    s -> fifo[172] = 0x000001f7;
    s -> fifo[173] = 0x00000001;
    s -> fifo[174] = 0x00000041;
    s -> fifo[175] = 0x00000041;
    s -> fifo[176] = 0x00000000;
    s -> fifo[177] = 0x000002e1;
    s -> fifo[178] = 0x000003e7;
    s -> fifo[179] = 0x000003e7;
    s -> fifo[180] = 0x000000e1;
    s -> fifo[181] = 0x000001e3;
    s -> fifo[182] = 0x000001e3;
    s -> fifo[183] = 0x000001e3;
    s -> fifo[184] = 0x000002e1;
    s -> fifo[185] = 0x000003e7;
    s -> fifo[186] = 0x000003f7;
    s -> fifo[187] = 0x000003e7;
    s -> fifo[188] = 0x000002e1;
    s -> fifo[189] = 0x000003e7;
    s -> fifo[190] = 0x000003e7;
    s -> fifo[191] = 0x00000261;
    s -> fifo[192] = 0x00000269;
    s -> fifo[193] = 0x00000063;
    s -> fifo[194] = 0x00000063;
    s -> fifo[195] = 0x000002e1;
    s -> fifo[196] = 0x000003e7;
    s -> fifo[197] = 0x000003f7;
    s -> fifo[198] = 0x000002e1;
    s -> fifo[199] = 0x000003f7;
    s -> fifo[200] = 0x000002f7;
    s -> fifo[201] = 0x000003e7;
    s -> fifo[202] = 0x000003e7;
    s -> fifo[203] = 0x000002e1;
    s -> fifo[204] = 0x000003e7;
    s -> fifo[205] = 0x000003e7;
    s -> fifo[206] = 0x000002e1;
    s -> fifo[207] = 0x00000269;
    s -> fifo[208] = 0x000003e7;
    s -> fifo[209] = 0x000003e7;
    s -> fifo[210] = 0x00000261;
    s -> fifo[211] = 0x00000269;
    s -> fifo[212] = 0x00000063;
    s -> fifo[213] = 0x00000063;
    s -> fifo[214] = 0x000002e1;
    s -> fifo[215] = 0x000003f7;
    s -> fifo[216] = 0x000003e7;
    s -> fifo[217] = 0x000003e7;
    s -> fifo[218] = 0x000002e1;
    s -> fifo[219] = 0x000003f7;
    s -> fifo[220] = 0x000003e7;
    s -> fifo[221] = 0x000003f7;
    s -> fifo[222] = 0x000003e7;
    s -> fifo[223] = 0x000002e1;
    s -> fifo[224] = 0x000003f7;
    s -> fifo[225] = 0x000003e7;
    s -> fifo[226] = 0x000003f7;
    s -> fifo[227] = 0x000003e7;
    s -> fifo[228] = 0x00000001;
    s -> fifo[229] = 0x000000e3;
    s -> fifo[230] = 0x000000e3;
    s -> fifo[231] = 0x000000e3;
    s -> fifo[232] = 0x000000e1;
    s -> fifo[233] = 0x000000e3;
    s -> fifo[234] = 0x000000e1;
    s -> fifo[235] = 0x000000e3;
    s -> fifo[236] = 0x000000e1;
    s -> fifo[237] = 0x000000e3;
    s -> fifo[238] = 0x000000e1;
    s -> fifo[239] = 0x00000063;
    s -> fifo[240] = 0x000000e3;
    s -> fifo[241] = 0x000000e1;
    s -> fifo[242] = 0x00000063;
    s -> fifo[243] = 0x000000e3;
    s -> fifo[244] = 0x00000045;
    s -> fifo[245] = 0x000002e1;
    s -> fifo[246] = 0x000002f7;
    s -> fifo[247] = 0x000002e1;
    s -> fifo[248] = 0x000002f7;
    s -> fifo[249] = 0x0000006b;
    s -> fifo[250] = 0x0000006b;
    s -> fifo[251] = 0x0000006b;
    s -> fifo[252] = 0x00000001;
    s -> fifo[253] = 0x000003f7;
    s -> fifo[254] = 0x000003f7;
    s -> fifo[255] = 0x000003f7;
    s -> fifo[256] = 0x000003f7;
    s -> fifo[257] = 0x000003f7;
    s -> fifo[258] = 0x000003f7;
    s -> fifo[259] = 0x000003f7;
    s -> fifo[260] = 0x000003f7;
    s -> fifo[261] = 0x000003f7;
    s -> fifo[262] = 0x000003f7;
    s -> fifo[263] = 0x000003f7;
    s -> fifo[264] = 0x000003f7;
    s -> fifo[265] = 0x00000269;
    s -> fifo[266] = 0x000002f7;
    s -> fifo[267] = 0x000000e3;
    s -> fifo[268] = 0x000000e3;
    s -> fifo[269] = 0x000000e3;
    s -> fifo[270] = 0x000002f7;
    s -> fifo[271] = 0x000002f7;
    s -> fifo[272] = 0x000003f7;
    s -> fifo[273] = 0x000003f7;
    s -> fifo[274] = 0x000000e3;
    s -> fifo[275] = 0x000000e3;
    s -> fifo[276] = 0x00000001;
    s -> fifo[277] = 0x00000001;
    s -> fifo[278] = 0x00000001;
    s -> fifo[279] = 0x00000001;
    s -> fifo[280] = 0x00000001;
    s -> fifo[281] = 0x00000001;
    s -> fifo[282] = 0x00000000;
    s -> fifo[283] = 0x000000e1;
    s -> fifo[284] = 0x000000e3;
    s -> fifo[285] = 0x000000e3;
    s -> fifo[286] = 0x000000e1;
    s -> fifo[287] = 0x000000e3;
    s -> fifo[288] = 0x000000e3;
    s -> fifo[289] = 0x00000000;
    s -> fifo[290] = 0x00000001;
    s -> fifo[291] = 0x00000001;
    s -> fifo[292] = 0x00000010;
    s -> fifo[293] = 0x00000001;
    if (s -> pitchlock != 0) {
        s -> fifo[SVGA_FIFO_PITCHLOCK] = s -> pitchlock;
    } else {
        s -> fifo[SVGA_FIFO_PITCHLOCK] = (((s -> new_depth) * (s -> new_width)) / (8));
    }
    s -> fifo[SVGA_FIFO_3D_HWVERSION] = SVGA3D_HWVERSION_CURRENT;
    s -> fifo[SVGA_FIFO_3D_HWVERSION_REVISED] = SVGA3D_HWVERSION_CURRENT;
    #ifdef VERBOSE
    s -> fifo[SVGA_FIFO_FLAGS] = SVGA_FIFO_FLAG_ACCELFRONT;
    #else
    s -> fifo[SVGA_FIFO_FLAGS] = SVGA_FIFO_FLAG_NONE;
    #endif
    s -> fifo[SVGA_FIFO_BUSY] = s -> sync;
    //s -> fifo[SVGA_FIFO_CAPABILITIES] = 1919;
    fc = 4294967295;
    #ifdef VERBOSE
    #else
    fc -= SVGA_FIFO_CAP_SCREEN_OBJECT;
    fc -= SVGA_FIFO_CAP_GMR2;
    fc -= SVGA_FIFO_CAP_SCREEN_OBJECT_2;
    #endif
    s -> fifo[SVGA_FIFO_CAPABILITIES] = fc;
    if ((s -> enable >= 1 || s -> config >= 1) && (s -> new_width >= 1 && s -> new_height >= 1 && s -> new_depth >= 1)) {
      if (s -> pitchlock != 0) {
            s -> new_width = (((s -> pitchlock) * (8)) / (s -> new_depth));
      }
      dpy_gfx_update(s -> vga.con, cx, cy, s -> new_width, s -> new_height);
    };
  };
};
static uint32_t vmsvga_value_read(void * opaque, uint32_t address) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_value_read was just executed\n");
  #endif
  uint32_t caps;
  uint32_t cap2;
  struct vmsvga_state_s * s = opaque;
  uint32_t ret;
  #ifdef VERBOSE
  printf("%s: Unknown register %u\n", __func__, s -> index);
  #endif
  switch (s -> index) {
  case SVGA_REG_FENCE_GOAL:
    //ret = 0;
    ret = s -> fifo[SVGA_FIFO_FENCE_GOAL];
    #ifdef VERBOSE
    printf("%s: SVGA_REG_FENCE_GOAL register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_ID:
    //ret = -1879048190;
    ret = s -> svgaid;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_ID register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_ENABLE:
    //ret = 1;
    ret = s -> enable;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_ENABLE register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_WIDTH:
    //ret = 1024;
    ret = s -> new_width;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_WIDTH register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_HEIGHT:
    //ret = 768;
    ret = s -> new_height;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_HEIGHT register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MAX_WIDTH:
    ret = 8192;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MAX_WIDTH register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MAX_HEIGHT:
    ret = 8192;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MAX_HEIGHT register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_SCREENTARGET_MAX_WIDTH:
    ret = 8192;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_SCREENTARGET_MAX_WIDTH register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_SCREENTARGET_MAX_HEIGHT:
    ret = 8192;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_SCREENTARGET_MAX_HEIGHT register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_BITS_PER_PIXEL:
    //ret = 32;
    ret = s -> new_depth;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BITS_PER_PIXEL register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_HOST_BITS_PER_PIXEL:
    ret = 32;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_HOST_BITS_PER_PIXEL register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DEPTH:
    //ret = 0;
    if ((s -> new_depth) == (32)) {
      ret = 24;
    } else {
      ret = s -> new_depth;
    };
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DEPTH register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_PSEUDOCOLOR:
    //ret = 0;
    if (s -> new_depth == 8) {
      ret = 1;
    } else {
      ret = 0;
    };
    #ifdef VERBOSE
    printf("%s: SVGA_REG_PSEUDOCOLOR register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_RED_MASK:
    //ret = 16711680;
    if (s -> new_depth == 8) {
      ret = 0x00000007;
    } else if (s -> new_depth == 15) {
      ret = 0x0000001f;
    } else if (s -> new_depth == 16) {
      ret = 0x0000001f;
    } else {
      ret = 0x00ff0000;
    };
    #ifdef VERBOSE
    printf("%s: SVGA_REG_RED_MASK register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GREEN_MASK:
    //ret = 65280;
    if (s -> new_depth == 8) {
      ret = 0x00000038;
    } else if (s -> new_depth == 15) {
      ret = 0x000003e0;
    } else if (s -> new_depth == 16) {
      ret = 0x000007e0;
    } else {
      ret = 0x0000ff00;
    };
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GREEN_MASK register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_BLUE_MASK:
    //ret = 255;
    if (s -> new_depth == 8) {
      ret = 0x000000c0;
    } else if (s -> new_depth == 15) {
      ret = 0x00007c00;
    } else if (s -> new_depth == 16) {
      ret = 0x0000f800;
    } else {
      ret = 0x000000ff;
    };
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BLUE_MASK register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_BYTES_PER_LINE:
    //ret = 4096;
    if (s -> pitchlock != 0) {
        ret = s -> pitchlock;
    } else {
        ret = (((s -> new_depth) * (s -> new_width)) / (8));
    }
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BYTES_PER_LINE register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_FB_START: {
    //ret = -268435456;
    struct pci_vmsvga_state_s * pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
    ret = pci_get_bar_addr(PCI_DEVICE(pci_vmsvga), 1);
    #ifdef VERBOSE
    printf("%s: SVGA_REG_FB_START register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  }
  case SVGA_REG_FB_OFFSET:
    ret = 0;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_FB_OFFSET register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_BLANK_SCREEN_TARGETS:
    ret = 0;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BLANK_SCREEN_TARGETS register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_VRAM_SIZE:
    #ifdef VERBOSE
    ret = 4194304;
    #else
    ret = s -> vga.vram_size;
    #endif
    #ifdef VERBOSE
    printf("%s: SVGA_REG_VRAM_SIZE register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_FB_SIZE:
    //ret = 3145728;
    if (s -> pitchlock != 0) {
        ret = ((s -> new_height) * (s -> pitchlock));
    } else {
        ret = ((s -> new_height) * ((((s -> new_depth) * (s -> new_width)) / (8))));
    }
    #ifdef VERBOSE
    printf("%s: SVGA_REG_FB_SIZE register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MOB_MAX_SIZE:
    ret = 1073741824;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MOB_MAX_SIZE register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GBOBJECT_MEM_SIZE_KB:
    ret = 8388608;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GBOBJECT_MEM_SIZE_KB register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_SUGGESTED_GBOBJECT_MEM_SIZE_KB:
    //ret = 3145728;
    if (s -> pitchlock != 0) {
        ret = ((s -> new_height) * (s -> pitchlock));
    } else {
        ret = ((s -> new_height) * ((((s -> new_depth) * (s -> new_width)) / (8))));
    }
    #ifdef VERBOSE
    printf("%s: SVGA_REG_SUGGESTED_GBOBJECT_MEM_SIZE_KB register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MSHINT:
    ret = 0;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MSHINT register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MAX_PRIMARY_BOUNDING_BOX_MEM:
    ret = 134217728;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MAX_PRIMARY_BOUNDING_BOX_MEM register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_CAPABILITIES:
    //ret = 4261397474;
    caps = 4294967295;
    #ifdef VERBOSE
    #else
    caps -= SVGA_CAP_RECT_COPY;
    caps -= SVGA_CAP_8BIT_EMULATION;
    caps -= SVGA_CAP_GMR;
    caps -= SVGA_CAP_GMR2;
    caps -= SVGA_CAP_SCREEN_OBJECT_2;
    caps -= SVGA_CAP_COMMAND_BUFFERS;
    caps -= SVGA_CAP_CMD_BUFFERS_2;
    caps -= SVGA_CAP_GBOBJECTS;
    caps -= SVGA_CAP_CMD_BUFFERS_3;
    #endif
    ret = caps;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CAPABILITIES register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_CAP2:
    //ret = 389119;
    cap2 = 4294967295;
    #ifdef VERBOSE
    #else
    #endif
    ret = cap2;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CAP2 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MEM_START: {
    struct pci_vmsvga_state_s * pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
    //ret = -75497472;
    ret = pci_get_bar_addr(PCI_DEVICE(pci_vmsvga), 2);
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MEM_START register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  }
  case SVGA_REG_MEM_SIZE:
    //ret = 262144;
    ret = s -> fifo_size;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MEM_SIZE register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_CONFIG_DONE:
    //ret = 0;
    ret = s -> config;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CONFIG_DONE register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_SYNC:
    //ret = 0;
    ret = s -> sync;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_SYNC register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_BUSY:
    //ret = 0;
    ret = s -> sync;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BUSY register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GUEST_ID:
    //ret = 0;
    ret = s -> guest;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GUEST_ID register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_CURSOR_ID:
    //ret = 0;
    ret = s -> cursor;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_ID register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_CURSOR_X:
    //ret = 0;
    ret = s -> fifo[SVGA_FIFO_CURSOR_X];
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_X register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_CURSOR_Y:
    //ret = 0;
    ret = s -> fifo[SVGA_FIFO_CURSOR_Y];
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_Y register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_CURSOR_ON:
    //ret = 0;
    if ((s -> fifo[SVGA_FIFO_CURSOR_ON] == SVGA_CURSOR_ON_SHOW) || (s -> fifo[SVGA_FIFO_CURSOR_ON] == SVGA_CURSOR_ON_RESTORE_TO_FB)) {
        ret = SVGA_CURSOR_ON_SHOW;
    } else {
        ret = SVGA_CURSOR_ON_HIDE;
    }
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_ON register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_SCRATCH_SIZE:
    //ret = 64;
    ret = s -> scratch_size;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_SCRATCH_SIZE register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MEM_REGS:
    ret = 291;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MEM_REGS register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_NUM_DISPLAYS:
    //ret = 10;
    ret = 1;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_NUM_DISPLAYS register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_PITCHLOCK:
    //ret = 0;
    if (s -> pitchlock != 0) {
        ret = s -> pitchlock;
    } else {
        ret = (((s -> new_depth) * (s -> new_width)) / (8));
    }
    #ifdef VERBOSE
    printf("%s: SVGA_REG_PITCHLOCK register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_IRQMASK:
    //ret = 0;
    ret = s -> irq_mask;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_IRQMASK register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_NUM_GUEST_DISPLAYS:
    //ret = 0;
    ret = 1;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_NUM_GUEST_DISPLAYS register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DISPLAY_ID:
    //ret = 0;
    ret = s -> display_id;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_ID register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DISPLAY_IS_PRIMARY:
    //ret = 0;
    ret = s -> disp_prim;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_IS_PRIMARY register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DISPLAY_POSITION_X:
    //ret = 0;
    ret = s -> disp_x;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_POSITION_X register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DISPLAY_POSITION_Y:
    //ret = 0;
    ret = s -> disp_y;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_POSITION_Y register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DISPLAY_WIDTH:
    //ret = 0;
    ret = s -> new_width;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_WIDTH register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DISPLAY_HEIGHT:
    //ret = 0;
    ret = s -> new_height;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_HEIGHT register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GMRS_MAX_PAGES:
    ret = 65536;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GMRS_MAX_PAGES register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GMR_ID:
    //ret = 0;
    ret = s -> gmrid;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GMR_ID register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GMR_MAX_IDS:
    ret = 64;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GMR_MAX_IDS register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH:
    ret = 4096;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_TRACES:
    ret = 0;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_TRACES register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_COMMAND_LOW:
    //ret = 0;
    ret = s -> cmd_low;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_COMMAND_LOW register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_COMMAND_HIGH:
    //ret = 0;
    ret = s -> cmd_high;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_COMMAND_HIGH register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_DEV_CAP:
    ret = s -> devcap_val;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DEV_CAP register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_REG_MEMORY_SIZE:
    //ret = 1073741824;
    ret = 4194304;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_MEMORY_SIZE register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_0:
    ret = s -> svgapalettebase0;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_0 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_1:
    ret = s -> svgapalettebase1;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_1 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_2:
    ret = s -> svgapalettebase2;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_2 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_3:
    ret = s -> svgapalettebase3;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_3 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_4:
    ret = s -> svgapalettebase4;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_4 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_5:
    ret = s -> svgapalettebase5;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_5 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_6:
    ret = s -> svgapalettebase6;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_6 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_7:
    ret = s -> svgapalettebase7;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_7 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_8:
    ret = s -> svgapalettebase8;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_8 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_9:
    ret = s -> svgapalettebase9;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_9 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_10:
    ret = s -> svgapalettebase10;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_10 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_11:
    ret = s -> svgapalettebase11;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_11 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_12:
    ret = s -> svgapalettebase12;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_12 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_13:
    ret = s -> svgapalettebase13;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_13 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_14:
    ret = s -> svgapalettebase14;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_14 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_15:
    ret = s -> svgapalettebase15;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_15 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_16:
    ret = s -> svgapalettebase16;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_16 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_17:
    ret = s -> svgapalettebase17;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_17 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_18:
    ret = s -> svgapalettebase18;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_18 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_19:
    ret = s -> svgapalettebase19;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_19 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_20:
    ret = s -> svgapalettebase20;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_20 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_21:
    ret = s -> svgapalettebase21;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_21 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_22:
    ret = s -> svgapalettebase22;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_22 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_23:
    ret = s -> svgapalettebase23;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_23 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_24:
    ret = s -> svgapalettebase24;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_24 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_25:
    ret = s -> svgapalettebase25;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_25 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_26:
    ret = s -> svgapalettebase26;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_26 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_27:
    ret = s -> svgapalettebase27;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_27 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_28:
    ret = s -> svgapalettebase28;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_28 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_29:
    ret = s -> svgapalettebase29;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_29 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_30:
    ret = s -> svgapalettebase30;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_30 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_31:
    ret = s -> svgapalettebase31;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_31 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_32:
    ret = s -> svgapalettebase32;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_32 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_33:
    ret = s -> svgapalettebase33;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_33 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_34:
    ret = s -> svgapalettebase34;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_34 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_35:
    ret = s -> svgapalettebase35;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_35 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_36:
    ret = s -> svgapalettebase36;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_36 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_37:
    ret = s -> svgapalettebase37;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_37 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_38:
    ret = s -> svgapalettebase38;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_38 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_39:
    ret = s -> svgapalettebase39;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_39 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_40:
    ret = s -> svgapalettebase40;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_40 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_41:
    ret = s -> svgapalettebase41;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_41 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_42:
    ret = s -> svgapalettebase42;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_42 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_43:
    ret = s -> svgapalettebase43;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_43 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_44:
    ret = s -> svgapalettebase44;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_44 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_45:
    ret = s -> svgapalettebase45;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_45 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_46:
    ret = s -> svgapalettebase46;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_46 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_47:
    ret = s -> svgapalettebase47;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_47 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_48:
    ret = s -> svgapalettebase48;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_48 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_49:
    ret = s -> svgapalettebase49;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_49 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_50:
    ret = s -> svgapalettebase50;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_50 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_51:
    ret = s -> svgapalettebase51;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_51 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_52:
    ret = s -> svgapalettebase52;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_52 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_53:
    ret = s -> svgapalettebase53;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_53 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_54:
    ret = s -> svgapalettebase54;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_54 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_55:
    ret = s -> svgapalettebase55;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_55 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_56:
    ret = s -> svgapalettebase56;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_56 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_57:
    ret = s -> svgapalettebase57;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_57 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_58:
    ret = s -> svgapalettebase58;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_58 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_59:
    ret = s -> svgapalettebase59;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_59 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_60:
    ret = s -> svgapalettebase60;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_60 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_61:
    ret = s -> svgapalettebase61;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_61 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_62:
    ret = s -> svgapalettebase62;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_62 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_63:
    ret = s -> svgapalettebase63;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_63 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_64:
    ret = s -> svgapalettebase64;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_64 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_65:
    ret = s -> svgapalettebase65;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_65 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_66:
    ret = s -> svgapalettebase66;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_66 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_67:
    ret = s -> svgapalettebase67;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_67 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_68:
    ret = s -> svgapalettebase68;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_68 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_69:
    ret = s -> svgapalettebase69;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_69 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_70:
    ret = s -> svgapalettebase70;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_70 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_71:
    ret = s -> svgapalettebase71;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_71 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_72:
    ret = s -> svgapalettebase72;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_72 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_73:
    ret = s -> svgapalettebase73;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_73 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_74:
    ret = s -> svgapalettebase74;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_74 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_75:
    ret = s -> svgapalettebase75;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_75 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_76:
    ret = s -> svgapalettebase76;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_76 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_77:
    ret = s -> svgapalettebase77;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_77 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_78:
    ret = s -> svgapalettebase78;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_78 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_79:
    ret = s -> svgapalettebase79;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_79 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_80:
    ret = s -> svgapalettebase80;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_80 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_81:
    ret = s -> svgapalettebase81;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_81 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_82:
    ret = s -> svgapalettebase82;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_82 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_83:
    ret = s -> svgapalettebase83;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_83 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_84:
    ret = s -> svgapalettebase84;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_84 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_85:
    ret = s -> svgapalettebase85;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_85 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_86:
    ret = s -> svgapalettebase86;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_86 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_87:
    ret = s -> svgapalettebase87;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_87 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_88:
    ret = s -> svgapalettebase88;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_88 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_89:
    ret = s -> svgapalettebase89;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_89 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_90:
    ret = s -> svgapalettebase90;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_90 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_91:
    ret = s -> svgapalettebase91;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_91 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_92:
    ret = s -> svgapalettebase92;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_92 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_93:
    ret = s -> svgapalettebase93;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_93 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_94:
    ret = s -> svgapalettebase94;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_94 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_95:
    ret = s -> svgapalettebase95;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_95 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_96:
    ret = s -> svgapalettebase96;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_96 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_97:
    ret = s -> svgapalettebase97;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_97 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_98:
    ret = s -> svgapalettebase98;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_98 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_99:
    ret = s -> svgapalettebase99;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_99 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_100:
    ret = s -> svgapalettebase100;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_100 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_101:
    ret = s -> svgapalettebase101;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_101 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_102:
    ret = s -> svgapalettebase102;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_102 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_103:
    ret = s -> svgapalettebase103;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_103 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_104:
    ret = s -> svgapalettebase104;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_104 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_105:
    ret = s -> svgapalettebase105;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_105 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_106:
    ret = s -> svgapalettebase106;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_106 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_107:
    ret = s -> svgapalettebase107;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_107 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_108:
    ret = s -> svgapalettebase108;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_108 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_109:
    ret = s -> svgapalettebase109;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_109 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_110:
    ret = s -> svgapalettebase110;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_110 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_111:
    ret = s -> svgapalettebase111;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_111 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_112:
    ret = s -> svgapalettebase112;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_112 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_113:
    ret = s -> svgapalettebase113;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_113 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_114:
    ret = s -> svgapalettebase114;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_114 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_115:
    ret = s -> svgapalettebase115;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_115 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_116:
    ret = s -> svgapalettebase116;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_116 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_117:
    ret = s -> svgapalettebase117;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_117 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_118:
    ret = s -> svgapalettebase118;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_118 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_119:
    ret = s -> svgapalettebase119;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_119 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_120:
    ret = s -> svgapalettebase120;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_120 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_121:
    ret = s -> svgapalettebase121;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_121 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_122:
    ret = s -> svgapalettebase122;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_122 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_123:
    ret = s -> svgapalettebase123;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_123 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_124:
    ret = s -> svgapalettebase124;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_124 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_125:
    ret = s -> svgapalettebase125;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_125 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_126:
    ret = s -> svgapalettebase126;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_126 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_127:
    ret = s -> svgapalettebase127;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_127 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_128:
    ret = s -> svgapalettebase128;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_128 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_129:
    ret = s -> svgapalettebase129;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_129 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_130:
    ret = s -> svgapalettebase130;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_130 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_131:
    ret = s -> svgapalettebase131;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_131 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_132:
    ret = s -> svgapalettebase132;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_132 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_133:
    ret = s -> svgapalettebase133;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_133 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_134:
    ret = s -> svgapalettebase134;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_134 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_135:
    ret = s -> svgapalettebase135;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_135 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_136:
    ret = s -> svgapalettebase136;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_136 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_137:
    ret = s -> svgapalettebase137;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_137 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_138:
    ret = s -> svgapalettebase138;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_138 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_139:
    ret = s -> svgapalettebase139;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_139 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_140:
    ret = s -> svgapalettebase140;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_140 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_141:
    ret = s -> svgapalettebase141;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_141 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_142:
    ret = s -> svgapalettebase142;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_142 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_143:
    ret = s -> svgapalettebase143;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_143 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_144:
    ret = s -> svgapalettebase144;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_144 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_145:
    ret = s -> svgapalettebase145;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_145 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_146:
    ret = s -> svgapalettebase146;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_146 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_147:
    ret = s -> svgapalettebase147;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_147 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_148:
    ret = s -> svgapalettebase148;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_148 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_149:
    ret = s -> svgapalettebase149;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_149 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_150:
    ret = s -> svgapalettebase150;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_150 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_151:
    ret = s -> svgapalettebase151;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_151 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_152:
    ret = s -> svgapalettebase152;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_152 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_153:
    ret = s -> svgapalettebase153;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_153 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_154:
    ret = s -> svgapalettebase154;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_154 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_155:
    ret = s -> svgapalettebase155;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_155 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_156:
    ret = s -> svgapalettebase156;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_156 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_157:
    ret = s -> svgapalettebase157;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_157 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_158:
    ret = s -> svgapalettebase158;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_158 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_159:
    ret = s -> svgapalettebase159;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_159 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_160:
    ret = s -> svgapalettebase160;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_160 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_161:
    ret = s -> svgapalettebase161;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_161 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_162:
    ret = s -> svgapalettebase162;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_162 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_163:
    ret = s -> svgapalettebase163;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_163 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_164:
    ret = s -> svgapalettebase164;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_164 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_165:
    ret = s -> svgapalettebase165;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_165 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_166:
    ret = s -> svgapalettebase166;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_166 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_167:
    ret = s -> svgapalettebase167;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_167 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_168:
    ret = s -> svgapalettebase168;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_168 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_169:
    ret = s -> svgapalettebase169;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_169 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_170:
    ret = s -> svgapalettebase170;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_170 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_171:
    ret = s -> svgapalettebase171;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_171 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_172:
    ret = s -> svgapalettebase172;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_172 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_173:
    ret = s -> svgapalettebase173;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_173 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_174:
    ret = s -> svgapalettebase174;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_174 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_175:
    ret = s -> svgapalettebase175;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_175 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_176:
    ret = s -> svgapalettebase176;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_176 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_177:
    ret = s -> svgapalettebase177;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_177 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_178:
    ret = s -> svgapalettebase178;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_178 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_179:
    ret = s -> svgapalettebase179;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_179 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_180:
    ret = s -> svgapalettebase180;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_180 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_181:
    ret = s -> svgapalettebase181;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_181 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_182:
    ret = s -> svgapalettebase182;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_182 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_183:
    ret = s -> svgapalettebase183;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_183 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_184:
    ret = s -> svgapalettebase184;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_184 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_185:
    ret = s -> svgapalettebase185;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_185 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_186:
    ret = s -> svgapalettebase186;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_186 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_187:
    ret = s -> svgapalettebase187;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_187 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_188:
    ret = s -> svgapalettebase188;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_188 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_189:
    ret = s -> svgapalettebase189;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_189 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_190:
    ret = s -> svgapalettebase190;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_190 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_191:
    ret = s -> svgapalettebase191;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_191 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_192:
    ret = s -> svgapalettebase192;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_192 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_193:
    ret = s -> svgapalettebase193;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_193 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_194:
    ret = s -> svgapalettebase194;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_194 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_195:
    ret = s -> svgapalettebase195;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_195 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_196:
    ret = s -> svgapalettebase196;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_196 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_197:
    ret = s -> svgapalettebase197;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_197 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_198:
    ret = s -> svgapalettebase198;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_198 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_199:
    ret = s -> svgapalettebase199;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_199 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_200:
    ret = s -> svgapalettebase200;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_200 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_201:
    ret = s -> svgapalettebase201;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_201 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_202:
    ret = s -> svgapalettebase202;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_202 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_203:
    ret = s -> svgapalettebase203;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_203 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_204:
    ret = s -> svgapalettebase204;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_204 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_205:
    ret = s -> svgapalettebase205;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_205 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_206:
    ret = s -> svgapalettebase206;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_206 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_207:
    ret = s -> svgapalettebase207;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_207 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_208:
    ret = s -> svgapalettebase208;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_208 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_209:
    ret = s -> svgapalettebase209;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_209 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_210:
    ret = s -> svgapalettebase210;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_210 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_211:
    ret = s -> svgapalettebase211;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_211 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_212:
    ret = s -> svgapalettebase212;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_212 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_213:
    ret = s -> svgapalettebase213;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_213 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_214:
    ret = s -> svgapalettebase214;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_214 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_215:
    ret = s -> svgapalettebase215;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_215 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_216:
    ret = s -> svgapalettebase216;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_216 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_217:
    ret = s -> svgapalettebase217;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_217 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_218:
    ret = s -> svgapalettebase218;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_218 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_219:
    ret = s -> svgapalettebase219;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_219 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_220:
    ret = s -> svgapalettebase220;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_220 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_221:
    ret = s -> svgapalettebase221;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_221 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_222:
    ret = s -> svgapalettebase222;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_222 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_223:
    ret = s -> svgapalettebase223;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_223 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_224:
    ret = s -> svgapalettebase224;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_224 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_225:
    ret = s -> svgapalettebase225;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_225 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_226:
    ret = s -> svgapalettebase226;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_226 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_227:
    ret = s -> svgapalettebase227;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_227 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_228:
    ret = s -> svgapalettebase228;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_228 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_229:
    ret = s -> svgapalettebase229;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_229 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_230:
    ret = s -> svgapalettebase230;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_230 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_231:
    ret = s -> svgapalettebase231;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_231 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_232:
    ret = s -> svgapalettebase232;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_232 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_233:
    ret = s -> svgapalettebase233;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_233 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_234:
    ret = s -> svgapalettebase234;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_234 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_235:
    ret = s -> svgapalettebase235;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_235 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_236:
    ret = s -> svgapalettebase236;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_236 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_237:
    ret = s -> svgapalettebase237;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_237 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_238:
    ret = s -> svgapalettebase238;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_238 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_239:
    ret = s -> svgapalettebase239;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_239 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_240:
    ret = s -> svgapalettebase240;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_240 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_241:
    ret = s -> svgapalettebase241;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_241 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_242:
    ret = s -> svgapalettebase242;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_242 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_243:
    ret = s -> svgapalettebase243;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_243 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_244:
    ret = s -> svgapalettebase244;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_244 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_245:
    ret = s -> svgapalettebase245;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_245 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_246:
    ret = s -> svgapalettebase246;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_246 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_247:
    ret = s -> svgapalettebase247;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_247 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_248:
    ret = s -> svgapalettebase248;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_248 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_249:
    ret = s -> svgapalettebase249;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_249 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_250:
    ret = s -> svgapalettebase250;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_250 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_251:
    ret = s -> svgapalettebase251;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_251 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_252:
    ret = s -> svgapalettebase252;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_252 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_253:
    ret = s -> svgapalettebase253;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_253 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_254:
    ret = s -> svgapalettebase254;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_254 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_255:
    ret = s -> svgapalettebase255;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_255 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_256:
    ret = s -> svgapalettebase256;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_256 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_257:
    ret = s -> svgapalettebase257;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_257 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_258:
    ret = s -> svgapalettebase258;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_258 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_259:
    ret = s -> svgapalettebase259;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_259 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_260:
    ret = s -> svgapalettebase260;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_260 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_261:
    ret = s -> svgapalettebase261;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_261 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_262:
    ret = s -> svgapalettebase262;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_262 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_263:
    ret = s -> svgapalettebase263;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_263 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_264:
    ret = s -> svgapalettebase264;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_264 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_265:
    ret = s -> svgapalettebase265;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_265 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_266:
    ret = s -> svgapalettebase266;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_266 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_267:
    ret = s -> svgapalettebase267;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_267 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_268:
    ret = s -> svgapalettebase268;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_268 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_269:
    ret = s -> svgapalettebase269;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_269 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_270:
    ret = s -> svgapalettebase270;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_270 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_271:
    ret = s -> svgapalettebase271;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_271 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_272:
    ret = s -> svgapalettebase272;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_272 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_273:
    ret = s -> svgapalettebase273;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_273 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_274:
    ret = s -> svgapalettebase274;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_274 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_275:
    ret = s -> svgapalettebase275;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_275 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_276:
    ret = s -> svgapalettebase276;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_276 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_277:
    ret = s -> svgapalettebase277;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_277 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_278:
    ret = s -> svgapalettebase278;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_278 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_279:
    ret = s -> svgapalettebase279;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_279 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_280:
    ret = s -> svgapalettebase280;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_280 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_281:
    ret = s -> svgapalettebase281;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_281 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_282:
    ret = s -> svgapalettebase282;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_282 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_283:
    ret = s -> svgapalettebase283;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_283 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_284:
    ret = s -> svgapalettebase284;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_284 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_285:
    ret = s -> svgapalettebase285;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_285 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_286:
    ret = s -> svgapalettebase286;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_286 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_287:
    ret = s -> svgapalettebase287;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_287 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_288:
    ret = s -> svgapalettebase288;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_288 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_289:
    ret = s -> svgapalettebase289;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_289 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_290:
    ret = s -> svgapalettebase290;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_290 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_291:
    ret = s -> svgapalettebase291;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_291 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_292:
    ret = s -> svgapalettebase292;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_292 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_293:
    ret = s -> svgapalettebase293;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_293 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_294:
    ret = s -> svgapalettebase294;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_294 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_295:
    ret = s -> svgapalettebase295;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_295 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_296:
    ret = s -> svgapalettebase296;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_296 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_297:
    ret = s -> svgapalettebase297;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_297 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_298:
    ret = s -> svgapalettebase298;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_298 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_299:
    ret = s -> svgapalettebase299;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_299 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_300:
    ret = s -> svgapalettebase300;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_300 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_301:
    ret = s -> svgapalettebase301;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_301 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_302:
    ret = s -> svgapalettebase302;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_302 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_303:
    ret = s -> svgapalettebase303;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_303 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_304:
    ret = s -> svgapalettebase304;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_304 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_305:
    ret = s -> svgapalettebase305;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_305 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_306:
    ret = s -> svgapalettebase306;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_306 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_307:
    ret = s -> svgapalettebase307;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_307 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_308:
    ret = s -> svgapalettebase308;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_308 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_309:
    ret = s -> svgapalettebase309;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_309 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_310:
    ret = s -> svgapalettebase310;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_310 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_311:
    ret = s -> svgapalettebase311;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_311 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_312:
    ret = s -> svgapalettebase312;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_312 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_313:
    ret = s -> svgapalettebase313;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_313 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_314:
    ret = s -> svgapalettebase314;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_314 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_315:
    ret = s -> svgapalettebase315;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_315 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_316:
    ret = s -> svgapalettebase316;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_316 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_317:
    ret = s -> svgapalettebase317;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_317 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_318:
    ret = s -> svgapalettebase318;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_318 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_319:
    ret = s -> svgapalettebase319;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_319 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_320:
    ret = s -> svgapalettebase320;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_320 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_321:
    ret = s -> svgapalettebase321;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_321 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_322:
    ret = s -> svgapalettebase322;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_322 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_323:
    ret = s -> svgapalettebase323;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_323 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_324:
    ret = s -> svgapalettebase324;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_324 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_325:
    ret = s -> svgapalettebase325;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_325 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_326:
    ret = s -> svgapalettebase326;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_326 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_327:
    ret = s -> svgapalettebase327;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_327 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_328:
    ret = s -> svgapalettebase328;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_328 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_329:
    ret = s -> svgapalettebase329;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_329 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_330:
    ret = s -> svgapalettebase330;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_330 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_331:
    ret = s -> svgapalettebase331;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_331 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_332:
    ret = s -> svgapalettebase332;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_332 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_333:
    ret = s -> svgapalettebase333;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_333 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_334:
    ret = s -> svgapalettebase334;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_334 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_335:
    ret = s -> svgapalettebase335;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_335 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_336:
    ret = s -> svgapalettebase336;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_336 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_337:
    ret = s -> svgapalettebase337;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_337 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_338:
    ret = s -> svgapalettebase338;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_338 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_339:
    ret = s -> svgapalettebase339;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_339 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_340:
    ret = s -> svgapalettebase340;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_340 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_341:
    ret = s -> svgapalettebase341;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_341 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_342:
    ret = s -> svgapalettebase342;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_342 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_343:
    ret = s -> svgapalettebase343;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_343 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_344:
    ret = s -> svgapalettebase344;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_344 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_345:
    ret = s -> svgapalettebase345;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_345 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_346:
    ret = s -> svgapalettebase346;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_346 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_347:
    ret = s -> svgapalettebase347;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_347 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_348:
    ret = s -> svgapalettebase348;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_348 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_349:
    ret = s -> svgapalettebase349;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_349 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_350:
    ret = s -> svgapalettebase350;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_350 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_351:
    ret = s -> svgapalettebase351;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_351 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_352:
    ret = s -> svgapalettebase352;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_352 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_353:
    ret = s -> svgapalettebase353;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_353 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_354:
    ret = s -> svgapalettebase354;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_354 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_355:
    ret = s -> svgapalettebase355;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_355 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_356:
    ret = s -> svgapalettebase356;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_356 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_357:
    ret = s -> svgapalettebase357;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_357 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_358:
    ret = s -> svgapalettebase358;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_358 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_359:
    ret = s -> svgapalettebase359;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_359 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_360:
    ret = s -> svgapalettebase360;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_360 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_361:
    ret = s -> svgapalettebase361;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_361 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_362:
    ret = s -> svgapalettebase362;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_362 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_363:
    ret = s -> svgapalettebase363;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_363 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_364:
    ret = s -> svgapalettebase364;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_364 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_365:
    ret = s -> svgapalettebase365;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_365 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_366:
    ret = s -> svgapalettebase366;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_366 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_367:
    ret = s -> svgapalettebase367;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_367 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_368:
    ret = s -> svgapalettebase368;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_368 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_369:
    ret = s -> svgapalettebase369;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_369 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_370:
    ret = s -> svgapalettebase370;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_370 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_371:
    ret = s -> svgapalettebase371;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_371 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_372:
    ret = s -> svgapalettebase372;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_372 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_373:
    ret = s -> svgapalettebase373;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_373 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_374:
    ret = s -> svgapalettebase374;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_374 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_375:
    ret = s -> svgapalettebase375;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_375 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_376:
    ret = s -> svgapalettebase376;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_376 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_377:
    ret = s -> svgapalettebase377;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_377 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_378:
    ret = s -> svgapalettebase378;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_378 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_379:
    ret = s -> svgapalettebase379;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_379 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_380:
    ret = s -> svgapalettebase380;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_380 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_381:
    ret = s -> svgapalettebase381;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_381 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_382:
    ret = s -> svgapalettebase382;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_382 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_383:
    ret = s -> svgapalettebase383;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_383 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_384:
    ret = s -> svgapalettebase384;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_384 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_385:
    ret = s -> svgapalettebase385;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_385 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_386:
    ret = s -> svgapalettebase386;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_386 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_387:
    ret = s -> svgapalettebase387;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_387 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_388:
    ret = s -> svgapalettebase388;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_388 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_389:
    ret = s -> svgapalettebase389;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_389 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_390:
    ret = s -> svgapalettebase390;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_390 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_391:
    ret = s -> svgapalettebase391;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_391 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_392:
    ret = s -> svgapalettebase392;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_392 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_393:
    ret = s -> svgapalettebase393;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_393 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_394:
    ret = s -> svgapalettebase394;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_394 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_395:
    ret = s -> svgapalettebase395;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_395 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_396:
    ret = s -> svgapalettebase396;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_396 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_397:
    ret = s -> svgapalettebase397;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_397 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_398:
    ret = s -> svgapalettebase398;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_398 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_399:
    ret = s -> svgapalettebase399;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_399 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_400:
    ret = s -> svgapalettebase400;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_400 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_401:
    ret = s -> svgapalettebase401;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_401 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_402:
    ret = s -> svgapalettebase402;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_402 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_403:
    ret = s -> svgapalettebase403;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_403 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_404:
    ret = s -> svgapalettebase404;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_404 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_405:
    ret = s -> svgapalettebase405;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_405 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_406:
    ret = s -> svgapalettebase406;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_406 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_407:
    ret = s -> svgapalettebase407;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_407 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_408:
    ret = s -> svgapalettebase408;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_408 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_409:
    ret = s -> svgapalettebase409;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_409 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_410:
    ret = s -> svgapalettebase410;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_410 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_411:
    ret = s -> svgapalettebase411;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_411 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_412:
    ret = s -> svgapalettebase412;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_412 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_413:
    ret = s -> svgapalettebase413;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_413 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_414:
    ret = s -> svgapalettebase414;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_414 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_415:
    ret = s -> svgapalettebase415;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_415 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_416:
    ret = s -> svgapalettebase416;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_416 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_417:
    ret = s -> svgapalettebase417;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_417 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_418:
    ret = s -> svgapalettebase418;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_418 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_419:
    ret = s -> svgapalettebase419;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_419 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_420:
    ret = s -> svgapalettebase420;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_420 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_421:
    ret = s -> svgapalettebase421;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_421 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_422:
    ret = s -> svgapalettebase422;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_422 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_423:
    ret = s -> svgapalettebase423;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_423 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_424:
    ret = s -> svgapalettebase424;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_424 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_425:
    ret = s -> svgapalettebase425;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_425 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_426:
    ret = s -> svgapalettebase426;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_426 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_427:
    ret = s -> svgapalettebase427;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_427 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_428:
    ret = s -> svgapalettebase428;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_428 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_429:
    ret = s -> svgapalettebase429;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_429 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_430:
    ret = s -> svgapalettebase430;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_430 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_431:
    ret = s -> svgapalettebase431;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_431 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_432:
    ret = s -> svgapalettebase432;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_432 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_433:
    ret = s -> svgapalettebase433;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_433 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_434:
    ret = s -> svgapalettebase434;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_434 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_435:
    ret = s -> svgapalettebase435;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_435 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_436:
    ret = s -> svgapalettebase436;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_436 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_437:
    ret = s -> svgapalettebase437;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_437 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_438:
    ret = s -> svgapalettebase438;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_438 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_439:
    ret = s -> svgapalettebase439;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_439 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_440:
    ret = s -> svgapalettebase440;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_440 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_441:
    ret = s -> svgapalettebase441;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_441 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_442:
    ret = s -> svgapalettebase442;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_442 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_443:
    ret = s -> svgapalettebase443;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_443 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_444:
    ret = s -> svgapalettebase444;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_444 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_445:
    ret = s -> svgapalettebase445;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_445 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_446:
    ret = s -> svgapalettebase446;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_446 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_447:
    ret = s -> svgapalettebase447;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_447 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_448:
    ret = s -> svgapalettebase448;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_448 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_449:
    ret = s -> svgapalettebase449;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_449 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_450:
    ret = s -> svgapalettebase450;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_450 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_451:
    ret = s -> svgapalettebase451;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_451 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_452:
    ret = s -> svgapalettebase452;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_452 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_453:
    ret = s -> svgapalettebase453;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_453 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_454:
    ret = s -> svgapalettebase454;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_454 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_455:
    ret = s -> svgapalettebase455;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_455 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_456:
    ret = s -> svgapalettebase456;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_456 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_457:
    ret = s -> svgapalettebase457;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_457 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_458:
    ret = s -> svgapalettebase458;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_458 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_459:
    ret = s -> svgapalettebase459;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_459 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_460:
    ret = s -> svgapalettebase460;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_460 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_461:
    ret = s -> svgapalettebase461;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_461 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_462:
    ret = s -> svgapalettebase462;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_462 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_463:
    ret = s -> svgapalettebase463;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_463 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_464:
    ret = s -> svgapalettebase464;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_464 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_465:
    ret = s -> svgapalettebase465;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_465 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_466:
    ret = s -> svgapalettebase466;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_466 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_467:
    ret = s -> svgapalettebase467;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_467 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_468:
    ret = s -> svgapalettebase468;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_468 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_469:
    ret = s -> svgapalettebase469;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_469 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_470:
    ret = s -> svgapalettebase470;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_470 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_471:
    ret = s -> svgapalettebase471;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_471 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_472:
    ret = s -> svgapalettebase472;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_472 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_473:
    ret = s -> svgapalettebase473;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_473 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_474:
    ret = s -> svgapalettebase474;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_474 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_475:
    ret = s -> svgapalettebase475;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_475 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_476:
    ret = s -> svgapalettebase476;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_476 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_477:
    ret = s -> svgapalettebase477;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_477 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_478:
    ret = s -> svgapalettebase478;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_478 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_479:
    ret = s -> svgapalettebase479;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_479 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_480:
    ret = s -> svgapalettebase480;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_480 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_481:
    ret = s -> svgapalettebase481;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_481 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_482:
    ret = s -> svgapalettebase482;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_482 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_483:
    ret = s -> svgapalettebase483;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_483 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_484:
    ret = s -> svgapalettebase484;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_484 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_485:
    ret = s -> svgapalettebase485;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_485 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_486:
    ret = s -> svgapalettebase486;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_486 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_487:
    ret = s -> svgapalettebase487;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_487 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_488:
    ret = s -> svgapalettebase488;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_488 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_489:
    ret = s -> svgapalettebase489;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_489 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_490:
    ret = s -> svgapalettebase490;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_490 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_491:
    ret = s -> svgapalettebase491;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_491 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_492:
    ret = s -> svgapalettebase492;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_492 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_493:
    ret = s -> svgapalettebase493;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_493 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_494:
    ret = s -> svgapalettebase494;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_494 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_495:
    ret = s -> svgapalettebase495;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_495 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_496:
    ret = s -> svgapalettebase496;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_496 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_497:
    ret = s -> svgapalettebase497;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_497 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_498:
    ret = s -> svgapalettebase498;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_498 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_499:
    ret = s -> svgapalettebase499;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_499 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_500:
    ret = s -> svgapalettebase500;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_500 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_501:
    ret = s -> svgapalettebase501;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_501 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_502:
    ret = s -> svgapalettebase502;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_502 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_503:
    ret = s -> svgapalettebase503;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_503 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_504:
    ret = s -> svgapalettebase504;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_504 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_505:
    ret = s -> svgapalettebase505;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_505 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_506:
    ret = s -> svgapalettebase506;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_506 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_507:
    ret = s -> svgapalettebase507;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_507 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_508:
    ret = s -> svgapalettebase508;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_508 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_509:
    ret = s -> svgapalettebase509;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_509 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_510:
    ret = s -> svgapalettebase510;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_510 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_511:
    ret = s -> svgapalettebase511;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_511 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_512:
    ret = s -> svgapalettebase512;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_512 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_513:
    ret = s -> svgapalettebase513;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_513 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_514:
    ret = s -> svgapalettebase514;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_514 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_515:
    ret = s -> svgapalettebase515;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_515 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_516:
    ret = s -> svgapalettebase516;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_516 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_517:
    ret = s -> svgapalettebase517;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_517 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_518:
    ret = s -> svgapalettebase518;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_518 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_519:
    ret = s -> svgapalettebase519;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_519 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_520:
    ret = s -> svgapalettebase520;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_520 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_521:
    ret = s -> svgapalettebase521;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_521 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_522:
    ret = s -> svgapalettebase522;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_522 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_523:
    ret = s -> svgapalettebase523;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_523 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_524:
    ret = s -> svgapalettebase524;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_524 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_525:
    ret = s -> svgapalettebase525;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_525 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_526:
    ret = s -> svgapalettebase526;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_526 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_527:
    ret = s -> svgapalettebase527;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_527 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_528:
    ret = s -> svgapalettebase528;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_528 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_529:
    ret = s -> svgapalettebase529;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_529 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_530:
    ret = s -> svgapalettebase530;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_530 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_531:
    ret = s -> svgapalettebase531;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_531 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_532:
    ret = s -> svgapalettebase532;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_532 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_533:
    ret = s -> svgapalettebase533;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_533 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_534:
    ret = s -> svgapalettebase534;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_534 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_535:
    ret = s -> svgapalettebase535;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_535 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_536:
    ret = s -> svgapalettebase536;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_536 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_537:
    ret = s -> svgapalettebase537;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_537 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_538:
    ret = s -> svgapalettebase538;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_538 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_539:
    ret = s -> svgapalettebase539;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_539 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_540:
    ret = s -> svgapalettebase540;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_540 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_541:
    ret = s -> svgapalettebase541;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_541 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_542:
    ret = s -> svgapalettebase542;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_542 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_543:
    ret = s -> svgapalettebase543;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_543 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_544:
    ret = s -> svgapalettebase544;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_544 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_545:
    ret = s -> svgapalettebase545;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_545 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_546:
    ret = s -> svgapalettebase546;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_546 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_547:
    ret = s -> svgapalettebase547;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_547 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_548:
    ret = s -> svgapalettebase548;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_548 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_549:
    ret = s -> svgapalettebase549;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_549 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_550:
    ret = s -> svgapalettebase550;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_550 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_551:
    ret = s -> svgapalettebase551;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_551 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_552:
    ret = s -> svgapalettebase552;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_552 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_553:
    ret = s -> svgapalettebase553;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_553 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_554:
    ret = s -> svgapalettebase554;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_554 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_555:
    ret = s -> svgapalettebase555;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_555 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_556:
    ret = s -> svgapalettebase556;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_556 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_557:
    ret = s -> svgapalettebase557;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_557 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_558:
    ret = s -> svgapalettebase558;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_558 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_559:
    ret = s -> svgapalettebase559;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_559 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_560:
    ret = s -> svgapalettebase560;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_560 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_561:
    ret = s -> svgapalettebase561;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_561 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_562:
    ret = s -> svgapalettebase562;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_562 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_563:
    ret = s -> svgapalettebase563;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_563 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_564:
    ret = s -> svgapalettebase564;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_564 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_565:
    ret = s -> svgapalettebase565;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_565 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_566:
    ret = s -> svgapalettebase566;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_566 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_567:
    ret = s -> svgapalettebase567;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_567 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_568:
    ret = s -> svgapalettebase568;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_568 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_569:
    ret = s -> svgapalettebase569;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_569 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_570:
    ret = s -> svgapalettebase570;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_570 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_571:
    ret = s -> svgapalettebase571;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_571 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_572:
    ret = s -> svgapalettebase572;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_572 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_573:
    ret = s -> svgapalettebase573;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_573 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_574:
    ret = s -> svgapalettebase574;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_574 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_575:
    ret = s -> svgapalettebase575;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_575 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_576:
    ret = s -> svgapalettebase576;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_576 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_577:
    ret = s -> svgapalettebase577;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_577 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_578:
    ret = s -> svgapalettebase578;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_578 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_579:
    ret = s -> svgapalettebase579;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_579 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_580:
    ret = s -> svgapalettebase580;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_580 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_581:
    ret = s -> svgapalettebase581;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_581 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_582:
    ret = s -> svgapalettebase582;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_582 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_583:
    ret = s -> svgapalettebase583;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_583 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_584:
    ret = s -> svgapalettebase584;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_584 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_585:
    ret = s -> svgapalettebase585;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_585 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_586:
    ret = s -> svgapalettebase586;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_586 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_587:
    ret = s -> svgapalettebase587;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_587 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_588:
    ret = s -> svgapalettebase588;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_588 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_589:
    ret = s -> svgapalettebase589;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_589 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_590:
    ret = s -> svgapalettebase590;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_590 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_591:
    ret = s -> svgapalettebase591;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_591 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_592:
    ret = s -> svgapalettebase592;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_592 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_593:
    ret = s -> svgapalettebase593;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_593 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_594:
    ret = s -> svgapalettebase594;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_594 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_595:
    ret = s -> svgapalettebase595;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_595 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_596:
    ret = s -> svgapalettebase596;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_596 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_597:
    ret = s -> svgapalettebase597;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_597 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_598:
    ret = s -> svgapalettebase598;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_598 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_599:
    ret = s -> svgapalettebase599;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_599 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_600:
    ret = s -> svgapalettebase600;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_600 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_601:
    ret = s -> svgapalettebase601;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_601 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_602:
    ret = s -> svgapalettebase602;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_602 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_603:
    ret = s -> svgapalettebase603;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_603 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_604:
    ret = s -> svgapalettebase604;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_604 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_605:
    ret = s -> svgapalettebase605;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_605 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_606:
    ret = s -> svgapalettebase606;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_606 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_607:
    ret = s -> svgapalettebase607;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_607 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_608:
    ret = s -> svgapalettebase608;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_608 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_609:
    ret = s -> svgapalettebase609;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_609 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_610:
    ret = s -> svgapalettebase610;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_610 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_611:
    ret = s -> svgapalettebase611;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_611 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_612:
    ret = s -> svgapalettebase612;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_612 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_613:
    ret = s -> svgapalettebase613;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_613 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_614:
    ret = s -> svgapalettebase614;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_614 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_615:
    ret = s -> svgapalettebase615;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_615 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_616:
    ret = s -> svgapalettebase616;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_616 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_617:
    ret = s -> svgapalettebase617;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_617 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_618:
    ret = s -> svgapalettebase618;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_618 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_619:
    ret = s -> svgapalettebase619;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_619 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_620:
    ret = s -> svgapalettebase620;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_620 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_621:
    ret = s -> svgapalettebase621;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_621 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_622:
    ret = s -> svgapalettebase622;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_622 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_623:
    ret = s -> svgapalettebase623;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_623 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_624:
    ret = s -> svgapalettebase624;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_624 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_625:
    ret = s -> svgapalettebase625;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_625 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_626:
    ret = s -> svgapalettebase626;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_626 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_627:
    ret = s -> svgapalettebase627;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_627 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_628:
    ret = s -> svgapalettebase628;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_628 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_629:
    ret = s -> svgapalettebase629;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_629 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_630:
    ret = s -> svgapalettebase630;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_630 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_631:
    ret = s -> svgapalettebase631;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_631 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_632:
    ret = s -> svgapalettebase632;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_632 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_633:
    ret = s -> svgapalettebase633;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_633 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_634:
    ret = s -> svgapalettebase634;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_634 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_635:
    ret = s -> svgapalettebase635;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_635 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_636:
    ret = s -> svgapalettebase636;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_636 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_637:
    ret = s -> svgapalettebase637;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_637 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_638:
    ret = s -> svgapalettebase638;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_638 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_639:
    ret = s -> svgapalettebase639;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_639 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_640:
    ret = s -> svgapalettebase640;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_640 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_641:
    ret = s -> svgapalettebase641;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_641 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_642:
    ret = s -> svgapalettebase642;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_642 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_643:
    ret = s -> svgapalettebase643;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_643 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_644:
    ret = s -> svgapalettebase644;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_644 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_645:
    ret = s -> svgapalettebase645;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_645 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_646:
    ret = s -> svgapalettebase646;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_646 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_647:
    ret = s -> svgapalettebase647;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_647 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_648:
    ret = s -> svgapalettebase648;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_648 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_649:
    ret = s -> svgapalettebase649;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_649 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_650:
    ret = s -> svgapalettebase650;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_650 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_651:
    ret = s -> svgapalettebase651;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_651 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_652:
    ret = s -> svgapalettebase652;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_652 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_653:
    ret = s -> svgapalettebase653;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_653 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_654:
    ret = s -> svgapalettebase654;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_654 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_655:
    ret = s -> svgapalettebase655;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_655 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_656:
    ret = s -> svgapalettebase656;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_656 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_657:
    ret = s -> svgapalettebase657;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_657 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_658:
    ret = s -> svgapalettebase658;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_658 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_659:
    ret = s -> svgapalettebase659;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_659 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_660:
    ret = s -> svgapalettebase660;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_660 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_661:
    ret = s -> svgapalettebase661;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_661 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_662:
    ret = s -> svgapalettebase662;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_662 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_663:
    ret = s -> svgapalettebase663;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_663 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_664:
    ret = s -> svgapalettebase664;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_664 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_665:
    ret = s -> svgapalettebase665;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_665 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_666:
    ret = s -> svgapalettebase666;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_666 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_667:
    ret = s -> svgapalettebase667;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_667 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_668:
    ret = s -> svgapalettebase668;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_668 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_669:
    ret = s -> svgapalettebase669;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_669 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_670:
    ret = s -> svgapalettebase670;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_670 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_671:
    ret = s -> svgapalettebase671;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_671 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_672:
    ret = s -> svgapalettebase672;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_672 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_673:
    ret = s -> svgapalettebase673;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_673 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_674:
    ret = s -> svgapalettebase674;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_674 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_675:
    ret = s -> svgapalettebase675;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_675 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_676:
    ret = s -> svgapalettebase676;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_676 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_677:
    ret = s -> svgapalettebase677;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_677 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_678:
    ret = s -> svgapalettebase678;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_678 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_679:
    ret = s -> svgapalettebase679;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_679 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_680:
    ret = s -> svgapalettebase680;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_680 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_681:
    ret = s -> svgapalettebase681;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_681 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_682:
    ret = s -> svgapalettebase682;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_682 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_683:
    ret = s -> svgapalettebase683;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_683 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_684:
    ret = s -> svgapalettebase684;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_684 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_685:
    ret = s -> svgapalettebase685;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_685 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_686:
    ret = s -> svgapalettebase686;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_686 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_687:
    ret = s -> svgapalettebase687;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_687 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_688:
    ret = s -> svgapalettebase688;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_688 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_689:
    ret = s -> svgapalettebase689;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_689 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_690:
    ret = s -> svgapalettebase690;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_690 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_691:
    ret = s -> svgapalettebase691;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_691 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_692:
    ret = s -> svgapalettebase692;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_692 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_693:
    ret = s -> svgapalettebase693;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_693 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_694:
    ret = s -> svgapalettebase694;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_694 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_695:
    ret = s -> svgapalettebase695;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_695 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_696:
    ret = s -> svgapalettebase696;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_696 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_697:
    ret = s -> svgapalettebase697;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_697 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_698:
    ret = s -> svgapalettebase698;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_698 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_699:
    ret = s -> svgapalettebase699;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_699 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_700:
    ret = s -> svgapalettebase700;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_700 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_701:
    ret = s -> svgapalettebase701;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_701 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_702:
    ret = s -> svgapalettebase702;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_702 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_703:
    ret = s -> svgapalettebase703;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_703 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_704:
    ret = s -> svgapalettebase704;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_704 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_705:
    ret = s -> svgapalettebase705;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_705 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_706:
    ret = s -> svgapalettebase706;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_706 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_707:
    ret = s -> svgapalettebase707;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_707 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_708:
    ret = s -> svgapalettebase708;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_708 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_709:
    ret = s -> svgapalettebase709;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_709 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_710:
    ret = s -> svgapalettebase710;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_710 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_711:
    ret = s -> svgapalettebase711;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_711 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_712:
    ret = s -> svgapalettebase712;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_712 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_713:
    ret = s -> svgapalettebase713;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_713 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_714:
    ret = s -> svgapalettebase714;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_714 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_715:
    ret = s -> svgapalettebase715;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_715 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_716:
    ret = s -> svgapalettebase716;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_716 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_717:
    ret = s -> svgapalettebase717;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_717 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_718:
    ret = s -> svgapalettebase718;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_718 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_719:
    ret = s -> svgapalettebase719;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_719 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_720:
    ret = s -> svgapalettebase720;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_720 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_721:
    ret = s -> svgapalettebase721;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_721 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_722:
    ret = s -> svgapalettebase722;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_722 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_723:
    ret = s -> svgapalettebase723;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_723 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_724:
    ret = s -> svgapalettebase724;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_724 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_725:
    ret = s -> svgapalettebase725;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_725 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_726:
    ret = s -> svgapalettebase726;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_726 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_727:
    ret = s -> svgapalettebase727;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_727 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_728:
    ret = s -> svgapalettebase728;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_728 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_729:
    ret = s -> svgapalettebase729;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_729 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_730:
    ret = s -> svgapalettebase730;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_730 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_731:
    ret = s -> svgapalettebase731;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_731 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_732:
    ret = s -> svgapalettebase732;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_732 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_733:
    ret = s -> svgapalettebase733;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_733 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_734:
    ret = s -> svgapalettebase734;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_734 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_735:
    ret = s -> svgapalettebase735;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_735 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_736:
    ret = s -> svgapalettebase736;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_736 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_737:
    ret = s -> svgapalettebase737;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_737 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_738:
    ret = s -> svgapalettebase738;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_738 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_739:
    ret = s -> svgapalettebase739;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_739 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_740:
    ret = s -> svgapalettebase740;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_740 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_741:
    ret = s -> svgapalettebase741;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_741 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_742:
    ret = s -> svgapalettebase742;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_742 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_743:
    ret = s -> svgapalettebase743;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_743 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_744:
    ret = s -> svgapalettebase744;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_744 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_745:
    ret = s -> svgapalettebase745;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_745 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_746:
    ret = s -> svgapalettebase746;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_746 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_747:
    ret = s -> svgapalettebase747;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_747 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_748:
    ret = s -> svgapalettebase748;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_748 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_749:
    ret = s -> svgapalettebase749;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_749 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_750:
    ret = s -> svgapalettebase750;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_750 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_751:
    ret = s -> svgapalettebase751;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_751 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_752:
    ret = s -> svgapalettebase752;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_752 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_753:
    ret = s -> svgapalettebase753;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_753 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_754:
    ret = s -> svgapalettebase754;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_754 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_755:
    ret = s -> svgapalettebase755;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_755 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_756:
    ret = s -> svgapalettebase756;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_756 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_757:
    ret = s -> svgapalettebase757;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_757 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_758:
    ret = s -> svgapalettebase758;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_758 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_759:
    ret = s -> svgapalettebase759;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_759 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_760:
    ret = s -> svgapalettebase760;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_760 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_761:
    ret = s -> svgapalettebase761;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_761 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_762:
    ret = s -> svgapalettebase762;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_762 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_763:
    ret = s -> svgapalettebase763;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_763 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_764:
    ret = s -> svgapalettebase764;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_764 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_765:
    ret = s -> svgapalettebase765;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_765 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_766:
    ret = s -> svgapalettebase766;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_766 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_767:
    ret = s -> svgapalettebase767;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_767 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  case SVGA_PALETTE_BASE_768:
    ret = s -> svgapalettebase768;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_768 register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  default:
    ret = 0;
    #ifdef VERBOSE
    printf("%s: default register %u with the return of %u\n", __func__, s -> index, ret);
    #endif
    break;
  }
  return ret;
}
static void vmsvga_value_write(void * opaque, uint32_t address, uint32_t value) {
  struct vmsvga_state_s * s = opaque;
  #ifdef VERBOSE
  printf("%s: Unknown register %u with the value of %u\n", __func__, s -> index, value);
  #endif
  switch (s -> index) {
  case SVGA_REG_ID:
    s -> svgaid = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_ID register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_FENCE_GOAL:
    s -> fifo[SVGA_FIFO_FENCE_GOAL] = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_FENCE_GOAL register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_ENABLE:
    s -> enable = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_ENABLE register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_WIDTH:
    s -> new_width = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_WIDTH register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_HEIGHT:
    s -> new_height = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_HEIGHT register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_BITS_PER_PIXEL:
    s -> new_depth = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BITS_PER_PIXEL register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_CONFIG_DONE:
    s -> config = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CONFIG_DONE register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_SYNC:
    s -> syncing = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_SYNC register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_BUSY:
    s -> syncing = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BUSY register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_GUEST_ID:
    s -> guest = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GUEST_ID register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_CURSOR_ID:
    s -> cursor = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_ID register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_CURSOR_X:
    s -> fifo[SVGA_FIFO_CURSOR_X] = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_X register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_CURSOR_Y:
    s -> fifo[SVGA_FIFO_CURSOR_Y] = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_Y register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_CURSOR_ON:
    s -> fifo[SVGA_FIFO_CURSOR_ON] = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_CURSOR_ON register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_BYTES_PER_LINE:
    s -> pitchlock = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_BYTES_PER_LINE register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_PITCHLOCK:
    s -> pitchlock = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_PITCHLOCK register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_IRQMASK:
    s -> irq_mask = value;
    struct pci_vmsvga_state_s * pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
    PCIDevice * pci_dev = PCI_DEVICE(pci_vmsvga);
    if (((value & s -> irq_status))) {
      #ifdef VERBOSE
      printf("pci_set_irq=1\n");
      #endif
      pci_set_irq(pci_dev, 1);
    } else {
      #ifdef VERBOSE
      printf("pci_set_irq=0\n");
      #endif
      pci_set_irq(pci_dev, 0);
    }
    #ifdef VERBOSE
    printf("%s: SVGA_REG_IRQMASK register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_NUM_GUEST_DISPLAYS:
    s -> num_gd = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_NUM_GUEST_DISPLAYS register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_DISPLAY_IS_PRIMARY:
    s -> disp_prim = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_IS_PRIMARY register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_DISPLAY_POSITION_X:
    s -> disp_x = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_POSITION_X register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_DISPLAY_POSITION_Y:
    s -> disp_y = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_POSITION_Y register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_DISPLAY_ID:
    s -> display_id = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_ID register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_DISPLAY_WIDTH:
    s -> new_width = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_WIDTH register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_DISPLAY_HEIGHT:
    s -> new_height = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DISPLAY_HEIGHT register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_TRACES:
    s -> tracez = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_TRACES register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_COMMAND_LOW:
    s -> cmd_low = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_COMMAND_LOW register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_COMMAND_HIGH:
    s -> cmd_high = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_COMMAND_HIGH register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_GMR_ID:
    s -> gmrid = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GMR_ID register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_GMR_DESCRIPTOR:
    s -> gmrdesc = value;
    #ifdef VERBOSE
    printf("%s: SVGA_REG_GMR_DESCRIPTOR register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_0:
    s -> svgapalettebase0 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_0 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_1:
    s -> svgapalettebase1 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_1 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_2:
    s -> svgapalettebase2 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_2 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_3:
    s -> svgapalettebase3 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_3 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_4:
    s -> svgapalettebase4 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_4 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_5:
    s -> svgapalettebase5 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_5 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_6:
    s -> svgapalettebase6 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_6 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_7:
    s -> svgapalettebase7 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_7 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_8:
    s -> svgapalettebase8 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_8 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_9:
    s -> svgapalettebase9 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_9 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_10:
    s -> svgapalettebase10 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_10 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_11:
    s -> svgapalettebase11 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_11 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_12:
    s -> svgapalettebase12 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_12 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_13:
    s -> svgapalettebase13 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_13 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_14:
    s -> svgapalettebase14 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_14 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_15:
    s -> svgapalettebase15 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_15 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_16:
    s -> svgapalettebase16 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_16 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_17:
    s -> svgapalettebase17 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_17 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_18:
    s -> svgapalettebase18 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_18 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_19:
    s -> svgapalettebase19 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_19 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_20:
    s -> svgapalettebase20 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_20 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_21:
    s -> svgapalettebase21 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_21 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_22:
    s -> svgapalettebase22 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_22 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_23:
    s -> svgapalettebase23 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_23 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_24:
    s -> svgapalettebase24 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_24 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_25:
    s -> svgapalettebase25 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_25 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_26:
    s -> svgapalettebase26 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_26 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_27:
    s -> svgapalettebase27 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_27 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_28:
    s -> svgapalettebase28 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_28 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_29:
    s -> svgapalettebase29 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_29 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_30:
    s -> svgapalettebase30 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_30 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_31:
    s -> svgapalettebase31 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_31 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_32:
    s -> svgapalettebase32 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_32 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_33:
    s -> svgapalettebase33 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_33 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_34:
    s -> svgapalettebase34 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_34 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_35:
    s -> svgapalettebase35 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_35 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_36:
    s -> svgapalettebase36 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_36 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_37:
    s -> svgapalettebase37 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_37 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_38:
    s -> svgapalettebase38 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_38 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_39:
    s -> svgapalettebase39 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_39 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_40:
    s -> svgapalettebase40 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_40 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_41:
    s -> svgapalettebase41 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_41 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_42:
    s -> svgapalettebase42 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_42 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_43:
    s -> svgapalettebase43 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_43 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_44:
    s -> svgapalettebase44 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_44 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_45:
    s -> svgapalettebase45 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_45 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_46:
    s -> svgapalettebase46 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_46 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_47:
    s -> svgapalettebase47 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_47 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_48:
    s -> svgapalettebase48 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_48 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_49:
    s -> svgapalettebase49 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_49 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_50:
    s -> svgapalettebase50 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_50 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_51:
    s -> svgapalettebase51 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_51 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_52:
    s -> svgapalettebase52 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_52 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_53:
    s -> svgapalettebase53 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_53 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_54:
    s -> svgapalettebase54 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_54 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_55:
    s -> svgapalettebase55 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_55 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_56:
    s -> svgapalettebase56 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_56 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_57:
    s -> svgapalettebase57 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_57 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_58:
    s -> svgapalettebase58 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_58 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_59:
    s -> svgapalettebase59 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_59 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_60:
    s -> svgapalettebase60 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_60 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_61:
    s -> svgapalettebase61 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_61 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_62:
    s -> svgapalettebase62 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_62 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_63:
    s -> svgapalettebase63 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_63 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_64:
    s -> svgapalettebase64 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_64 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_65:
    s -> svgapalettebase65 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_65 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_66:
    s -> svgapalettebase66 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_66 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_67:
    s -> svgapalettebase67 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_67 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_68:
    s -> svgapalettebase68 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_68 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_69:
    s -> svgapalettebase69 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_69 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_70:
    s -> svgapalettebase70 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_70 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_71:
    s -> svgapalettebase71 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_71 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_72:
    s -> svgapalettebase72 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_72 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_73:
    s -> svgapalettebase73 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_73 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_74:
    s -> svgapalettebase74 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_74 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_75:
    s -> svgapalettebase75 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_75 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_76:
    s -> svgapalettebase76 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_76 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_77:
    s -> svgapalettebase77 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_77 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_78:
    s -> svgapalettebase78 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_78 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_79:
    s -> svgapalettebase79 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_79 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_80:
    s -> svgapalettebase80 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_80 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_81:
    s -> svgapalettebase81 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_81 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_82:
    s -> svgapalettebase82 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_82 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_83:
    s -> svgapalettebase83 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_83 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_84:
    s -> svgapalettebase84 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_84 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_85:
    s -> svgapalettebase85 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_85 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_86:
    s -> svgapalettebase86 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_86 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_87:
    s -> svgapalettebase87 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_87 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_88:
    s -> svgapalettebase88 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_88 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_89:
    s -> svgapalettebase89 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_89 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_90:
    s -> svgapalettebase90 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_90 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_91:
    s -> svgapalettebase91 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_91 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_92:
    s -> svgapalettebase92 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_92 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_93:
    s -> svgapalettebase93 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_93 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_94:
    s -> svgapalettebase94 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_94 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_95:
    s -> svgapalettebase95 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_95 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_96:
    s -> svgapalettebase96 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_96 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_97:
    s -> svgapalettebase97 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_97 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_98:
    s -> svgapalettebase98 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_98 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_99:
    s -> svgapalettebase99 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_99 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_100:
    s -> svgapalettebase100 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_100 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_101:
    s -> svgapalettebase101 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_101 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_102:
    s -> svgapalettebase102 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_102 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_103:
    s -> svgapalettebase103 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_103 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_104:
    s -> svgapalettebase104 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_104 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_105:
    s -> svgapalettebase105 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_105 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_106:
    s -> svgapalettebase106 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_106 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_107:
    s -> svgapalettebase107 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_107 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_108:
    s -> svgapalettebase108 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_108 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_109:
    s -> svgapalettebase109 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_109 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_110:
    s -> svgapalettebase110 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_110 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_111:
    s -> svgapalettebase111 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_111 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_112:
    s -> svgapalettebase112 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_112 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_113:
    s -> svgapalettebase113 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_113 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_114:
    s -> svgapalettebase114 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_114 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_115:
    s -> svgapalettebase115 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_115 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_116:
    s -> svgapalettebase116 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_116 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_117:
    s -> svgapalettebase117 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_117 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_118:
    s -> svgapalettebase118 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_118 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_119:
    s -> svgapalettebase119 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_119 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_120:
    s -> svgapalettebase120 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_120 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_121:
    s -> svgapalettebase121 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_121 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_122:
    s -> svgapalettebase122 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_122 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_123:
    s -> svgapalettebase123 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_123 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_124:
    s -> svgapalettebase124 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_124 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_125:
    s -> svgapalettebase125 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_125 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_126:
    s -> svgapalettebase126 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_126 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_127:
    s -> svgapalettebase127 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_127 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_128:
    s -> svgapalettebase128 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_128 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_129:
    s -> svgapalettebase129 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_129 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_130:
    s -> svgapalettebase130 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_130 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_131:
    s -> svgapalettebase131 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_131 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_132:
    s -> svgapalettebase132 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_132 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_133:
    s -> svgapalettebase133 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_133 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_134:
    s -> svgapalettebase134 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_134 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_135:
    s -> svgapalettebase135 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_135 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_136:
    s -> svgapalettebase136 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_136 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_137:
    s -> svgapalettebase137 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_137 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_138:
    s -> svgapalettebase138 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_138 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_139:
    s -> svgapalettebase139 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_139 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_140:
    s -> svgapalettebase140 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_140 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_141:
    s -> svgapalettebase141 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_141 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_142:
    s -> svgapalettebase142 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_142 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_143:
    s -> svgapalettebase143 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_143 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_144:
    s -> svgapalettebase144 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_144 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_145:
    s -> svgapalettebase145 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_145 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_146:
    s -> svgapalettebase146 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_146 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_147:
    s -> svgapalettebase147 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_147 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_148:
    s -> svgapalettebase148 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_148 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_149:
    s -> svgapalettebase149 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_149 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_150:
    s -> svgapalettebase150 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_150 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_151:
    s -> svgapalettebase151 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_151 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_152:
    s -> svgapalettebase152 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_152 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_153:
    s -> svgapalettebase153 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_153 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_154:
    s -> svgapalettebase154 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_154 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_155:
    s -> svgapalettebase155 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_155 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_156:
    s -> svgapalettebase156 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_156 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_157:
    s -> svgapalettebase157 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_157 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_158:
    s -> svgapalettebase158 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_158 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_159:
    s -> svgapalettebase159 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_159 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_160:
    s -> svgapalettebase160 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_160 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_161:
    s -> svgapalettebase161 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_161 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_162:
    s -> svgapalettebase162 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_162 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_163:
    s -> svgapalettebase163 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_163 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_164:
    s -> svgapalettebase164 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_164 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_165:
    s -> svgapalettebase165 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_165 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_166:
    s -> svgapalettebase166 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_166 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_167:
    s -> svgapalettebase167 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_167 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_168:
    s -> svgapalettebase168 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_168 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_169:
    s -> svgapalettebase169 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_169 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_170:
    s -> svgapalettebase170 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_170 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_171:
    s -> svgapalettebase171 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_171 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_172:
    s -> svgapalettebase172 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_172 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_173:
    s -> svgapalettebase173 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_173 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_174:
    s -> svgapalettebase174 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_174 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_175:
    s -> svgapalettebase175 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_175 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_176:
    s -> svgapalettebase176 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_176 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_177:
    s -> svgapalettebase177 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_177 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_178:
    s -> svgapalettebase178 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_178 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_179:
    s -> svgapalettebase179 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_179 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_180:
    s -> svgapalettebase180 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_180 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_181:
    s -> svgapalettebase181 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_181 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_182:
    s -> svgapalettebase182 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_182 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_183:
    s -> svgapalettebase183 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_183 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_184:
    s -> svgapalettebase184 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_184 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_185:
    s -> svgapalettebase185 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_185 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_186:
    s -> svgapalettebase186 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_186 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_187:
    s -> svgapalettebase187 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_187 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_188:
    s -> svgapalettebase188 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_188 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_189:
    s -> svgapalettebase189 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_189 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_190:
    s -> svgapalettebase190 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_190 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_191:
    s -> svgapalettebase191 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_191 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_192:
    s -> svgapalettebase192 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_192 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_193:
    s -> svgapalettebase193 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_193 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_194:
    s -> svgapalettebase194 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_194 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_195:
    s -> svgapalettebase195 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_195 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_196:
    s -> svgapalettebase196 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_196 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_197:
    s -> svgapalettebase197 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_197 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_198:
    s -> svgapalettebase198 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_198 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_199:
    s -> svgapalettebase199 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_199 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_200:
    s -> svgapalettebase200 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_200 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_201:
    s -> svgapalettebase201 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_201 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_202:
    s -> svgapalettebase202 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_202 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_203:
    s -> svgapalettebase203 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_203 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_204:
    s -> svgapalettebase204 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_204 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_205:
    s -> svgapalettebase205 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_205 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_206:
    s -> svgapalettebase206 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_206 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_207:
    s -> svgapalettebase207 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_207 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_208:
    s -> svgapalettebase208 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_208 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_209:
    s -> svgapalettebase209 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_209 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_210:
    s -> svgapalettebase210 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_210 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_211:
    s -> svgapalettebase211 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_211 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_212:
    s -> svgapalettebase212 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_212 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_213:
    s -> svgapalettebase213 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_213 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_214:
    s -> svgapalettebase214 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_214 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_215:
    s -> svgapalettebase215 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_215 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_216:
    s -> svgapalettebase216 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_216 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_217:
    s -> svgapalettebase217 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_217 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_218:
    s -> svgapalettebase218 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_218 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_219:
    s -> svgapalettebase219 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_219 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_220:
    s -> svgapalettebase220 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_220 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_221:
    s -> svgapalettebase221 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_221 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_222:
    s -> svgapalettebase222 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_222 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_223:
    s -> svgapalettebase223 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_223 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_224:
    s -> svgapalettebase224 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_224 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_225:
    s -> svgapalettebase225 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_225 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_226:
    s -> svgapalettebase226 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_226 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_227:
    s -> svgapalettebase227 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_227 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_228:
    s -> svgapalettebase228 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_228 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_229:
    s -> svgapalettebase229 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_229 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_230:
    s -> svgapalettebase230 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_230 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_231:
    s -> svgapalettebase231 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_231 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_232:
    s -> svgapalettebase232 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_232 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_233:
    s -> svgapalettebase233 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_233 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_234:
    s -> svgapalettebase234 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_234 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_235:
    s -> svgapalettebase235 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_235 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_236:
    s -> svgapalettebase236 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_236 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_237:
    s -> svgapalettebase237 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_237 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_238:
    s -> svgapalettebase238 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_238 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_239:
    s -> svgapalettebase239 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_239 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_240:
    s -> svgapalettebase240 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_240 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_241:
    s -> svgapalettebase241 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_241 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_242:
    s -> svgapalettebase242 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_242 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_243:
    s -> svgapalettebase243 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_243 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_244:
    s -> svgapalettebase244 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_244 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_245:
    s -> svgapalettebase245 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_245 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_246:
    s -> svgapalettebase246 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_246 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_247:
    s -> svgapalettebase247 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_247 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_248:
    s -> svgapalettebase248 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_248 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_249:
    s -> svgapalettebase249 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_249 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_250:
    s -> svgapalettebase250 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_250 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_251:
    s -> svgapalettebase251 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_251 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_252:
    s -> svgapalettebase252 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_252 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_253:
    s -> svgapalettebase253 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_253 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_254:
    s -> svgapalettebase254 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_254 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_255:
    s -> svgapalettebase255 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_255 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_256:
    s -> svgapalettebase256 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_256 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_257:
    s -> svgapalettebase257 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_257 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_258:
    s -> svgapalettebase258 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_258 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_259:
    s -> svgapalettebase259 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_259 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_260:
    s -> svgapalettebase260 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_260 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_261:
    s -> svgapalettebase261 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_261 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_262:
    s -> svgapalettebase262 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_262 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_263:
    s -> svgapalettebase263 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_263 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_264:
    s -> svgapalettebase264 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_264 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_265:
    s -> svgapalettebase265 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_265 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_266:
    s -> svgapalettebase266 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_266 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_267:
    s -> svgapalettebase267 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_267 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_268:
    s -> svgapalettebase268 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_268 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_269:
    s -> svgapalettebase269 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_269 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_270:
    s -> svgapalettebase270 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_270 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_271:
    s -> svgapalettebase271 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_271 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_272:
    s -> svgapalettebase272 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_272 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_273:
    s -> svgapalettebase273 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_273 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_274:
    s -> svgapalettebase274 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_274 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_275:
    s -> svgapalettebase275 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_275 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_276:
    s -> svgapalettebase276 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_276 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_277:
    s -> svgapalettebase277 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_277 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_278:
    s -> svgapalettebase278 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_278 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_279:
    s -> svgapalettebase279 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_279 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_280:
    s -> svgapalettebase280 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_280 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_281:
    s -> svgapalettebase281 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_281 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_282:
    s -> svgapalettebase282 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_282 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_283:
    s -> svgapalettebase283 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_283 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_284:
    s -> svgapalettebase284 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_284 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_285:
    s -> svgapalettebase285 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_285 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_286:
    s -> svgapalettebase286 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_286 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_287:
    s -> svgapalettebase287 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_287 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_288:
    s -> svgapalettebase288 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_288 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_289:
    s -> svgapalettebase289 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_289 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_290:
    s -> svgapalettebase290 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_290 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_291:
    s -> svgapalettebase291 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_291 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_292:
    s -> svgapalettebase292 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_292 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_293:
    s -> svgapalettebase293 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_293 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_294:
    s -> svgapalettebase294 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_294 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_295:
    s -> svgapalettebase295 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_295 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_296:
    s -> svgapalettebase296 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_296 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_297:
    s -> svgapalettebase297 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_297 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_298:
    s -> svgapalettebase298 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_298 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_299:
    s -> svgapalettebase299 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_299 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_300:
    s -> svgapalettebase300 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_300 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_301:
    s -> svgapalettebase301 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_301 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_302:
    s -> svgapalettebase302 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_302 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_303:
    s -> svgapalettebase303 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_303 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_304:
    s -> svgapalettebase304 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_304 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_305:
    s -> svgapalettebase305 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_305 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_306:
    s -> svgapalettebase306 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_306 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_307:
    s -> svgapalettebase307 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_307 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_308:
    s -> svgapalettebase308 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_308 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_309:
    s -> svgapalettebase309 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_309 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_310:
    s -> svgapalettebase310 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_310 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_311:
    s -> svgapalettebase311 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_311 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_312:
    s -> svgapalettebase312 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_312 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_313:
    s -> svgapalettebase313 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_313 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_314:
    s -> svgapalettebase314 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_314 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_315:
    s -> svgapalettebase315 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_315 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_316:
    s -> svgapalettebase316 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_316 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_317:
    s -> svgapalettebase317 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_317 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_318:
    s -> svgapalettebase318 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_318 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_319:
    s -> svgapalettebase319 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_319 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_320:
    s -> svgapalettebase320 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_320 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_321:
    s -> svgapalettebase321 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_321 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_322:
    s -> svgapalettebase322 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_322 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_323:
    s -> svgapalettebase323 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_323 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_324:
    s -> svgapalettebase324 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_324 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_325:
    s -> svgapalettebase325 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_325 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_326:
    s -> svgapalettebase326 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_326 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_327:
    s -> svgapalettebase327 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_327 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_328:
    s -> svgapalettebase328 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_328 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_329:
    s -> svgapalettebase329 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_329 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_330:
    s -> svgapalettebase330 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_330 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_331:
    s -> svgapalettebase331 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_331 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_332:
    s -> svgapalettebase332 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_332 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_333:
    s -> svgapalettebase333 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_333 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_334:
    s -> svgapalettebase334 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_334 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_335:
    s -> svgapalettebase335 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_335 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_336:
    s -> svgapalettebase336 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_336 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_337:
    s -> svgapalettebase337 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_337 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_338:
    s -> svgapalettebase338 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_338 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_339:
    s -> svgapalettebase339 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_339 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_340:
    s -> svgapalettebase340 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_340 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_341:
    s -> svgapalettebase341 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_341 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_342:
    s -> svgapalettebase342 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_342 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_343:
    s -> svgapalettebase343 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_343 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_344:
    s -> svgapalettebase344 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_344 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_345:
    s -> svgapalettebase345 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_345 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_346:
    s -> svgapalettebase346 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_346 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_347:
    s -> svgapalettebase347 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_347 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_348:
    s -> svgapalettebase348 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_348 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_349:
    s -> svgapalettebase349 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_349 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_350:
    s -> svgapalettebase350 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_350 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_351:
    s -> svgapalettebase351 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_351 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_352:
    s -> svgapalettebase352 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_352 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_353:
    s -> svgapalettebase353 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_353 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_354:
    s -> svgapalettebase354 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_354 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_355:
    s -> svgapalettebase355 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_355 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_356:
    s -> svgapalettebase356 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_356 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_357:
    s -> svgapalettebase357 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_357 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_358:
    s -> svgapalettebase358 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_358 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_359:
    s -> svgapalettebase359 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_359 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_360:
    s -> svgapalettebase360 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_360 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_361:
    s -> svgapalettebase361 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_361 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_362:
    s -> svgapalettebase362 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_362 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_363:
    s -> svgapalettebase363 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_363 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_364:
    s -> svgapalettebase364 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_364 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_365:
    s -> svgapalettebase365 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_365 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_366:
    s -> svgapalettebase366 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_366 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_367:
    s -> svgapalettebase367 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_367 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_368:
    s -> svgapalettebase368 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_368 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_369:
    s -> svgapalettebase369 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_369 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_370:
    s -> svgapalettebase370 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_370 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_371:
    s -> svgapalettebase371 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_371 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_372:
    s -> svgapalettebase372 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_372 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_373:
    s -> svgapalettebase373 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_373 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_374:
    s -> svgapalettebase374 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_374 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_375:
    s -> svgapalettebase375 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_375 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_376:
    s -> svgapalettebase376 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_376 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_377:
    s -> svgapalettebase377 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_377 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_378:
    s -> svgapalettebase378 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_378 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_379:
    s -> svgapalettebase379 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_379 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_380:
    s -> svgapalettebase380 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_380 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_381:
    s -> svgapalettebase381 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_381 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_382:
    s -> svgapalettebase382 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_382 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_383:
    s -> svgapalettebase383 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_383 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_384:
    s -> svgapalettebase384 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_384 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_385:
    s -> svgapalettebase385 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_385 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_386:
    s -> svgapalettebase386 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_386 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_387:
    s -> svgapalettebase387 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_387 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_388:
    s -> svgapalettebase388 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_388 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_389:
    s -> svgapalettebase389 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_389 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_390:
    s -> svgapalettebase390 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_390 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_391:
    s -> svgapalettebase391 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_391 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_392:
    s -> svgapalettebase392 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_392 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_393:
    s -> svgapalettebase393 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_393 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_394:
    s -> svgapalettebase394 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_394 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_395:
    s -> svgapalettebase395 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_395 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_396:
    s -> svgapalettebase396 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_396 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_397:
    s -> svgapalettebase397 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_397 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_398:
    s -> svgapalettebase398 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_398 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_399:
    s -> svgapalettebase399 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_399 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_400:
    s -> svgapalettebase400 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_400 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_401:
    s -> svgapalettebase401 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_401 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_402:
    s -> svgapalettebase402 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_402 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_403:
    s -> svgapalettebase403 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_403 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_404:
    s -> svgapalettebase404 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_404 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_405:
    s -> svgapalettebase405 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_405 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_406:
    s -> svgapalettebase406 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_406 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_407:
    s -> svgapalettebase407 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_407 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_408:
    s -> svgapalettebase408 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_408 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_409:
    s -> svgapalettebase409 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_409 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_410:
    s -> svgapalettebase410 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_410 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_411:
    s -> svgapalettebase411 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_411 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_412:
    s -> svgapalettebase412 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_412 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_413:
    s -> svgapalettebase413 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_413 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_414:
    s -> svgapalettebase414 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_414 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_415:
    s -> svgapalettebase415 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_415 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_416:
    s -> svgapalettebase416 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_416 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_417:
    s -> svgapalettebase417 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_417 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_418:
    s -> svgapalettebase418 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_418 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_419:
    s -> svgapalettebase419 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_419 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_420:
    s -> svgapalettebase420 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_420 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_421:
    s -> svgapalettebase421 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_421 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_422:
    s -> svgapalettebase422 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_422 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_423:
    s -> svgapalettebase423 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_423 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_424:
    s -> svgapalettebase424 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_424 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_425:
    s -> svgapalettebase425 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_425 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_426:
    s -> svgapalettebase426 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_426 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_427:
    s -> svgapalettebase427 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_427 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_428:
    s -> svgapalettebase428 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_428 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_429:
    s -> svgapalettebase429 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_429 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_430:
    s -> svgapalettebase430 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_430 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_431:
    s -> svgapalettebase431 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_431 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_432:
    s -> svgapalettebase432 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_432 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_433:
    s -> svgapalettebase433 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_433 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_434:
    s -> svgapalettebase434 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_434 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_435:
    s -> svgapalettebase435 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_435 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_436:
    s -> svgapalettebase436 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_436 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_437:
    s -> svgapalettebase437 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_437 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_438:
    s -> svgapalettebase438 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_438 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_439:
    s -> svgapalettebase439 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_439 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_440:
    s -> svgapalettebase440 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_440 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_441:
    s -> svgapalettebase441 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_441 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_442:
    s -> svgapalettebase442 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_442 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_443:
    s -> svgapalettebase443 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_443 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_444:
    s -> svgapalettebase444 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_444 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_445:
    s -> svgapalettebase445 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_445 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_446:
    s -> svgapalettebase446 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_446 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_447:
    s -> svgapalettebase447 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_447 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_448:
    s -> svgapalettebase448 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_448 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_449:
    s -> svgapalettebase449 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_449 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_450:
    s -> svgapalettebase450 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_450 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_451:
    s -> svgapalettebase451 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_451 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_452:
    s -> svgapalettebase452 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_452 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_453:
    s -> svgapalettebase453 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_453 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_454:
    s -> svgapalettebase454 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_454 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_455:
    s -> svgapalettebase455 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_455 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_456:
    s -> svgapalettebase456 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_456 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_457:
    s -> svgapalettebase457 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_457 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_458:
    s -> svgapalettebase458 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_458 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_459:
    s -> svgapalettebase459 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_459 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_460:
    s -> svgapalettebase460 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_460 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_461:
    s -> svgapalettebase461 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_461 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_462:
    s -> svgapalettebase462 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_462 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_463:
    s -> svgapalettebase463 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_463 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_464:
    s -> svgapalettebase464 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_464 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_465:
    s -> svgapalettebase465 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_465 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_466:
    s -> svgapalettebase466 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_466 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_467:
    s -> svgapalettebase467 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_467 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_468:
    s -> svgapalettebase468 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_468 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_469:
    s -> svgapalettebase469 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_469 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_470:
    s -> svgapalettebase470 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_470 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_471:
    s -> svgapalettebase471 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_471 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_472:
    s -> svgapalettebase472 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_472 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_473:
    s -> svgapalettebase473 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_473 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_474:
    s -> svgapalettebase474 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_474 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_475:
    s -> svgapalettebase475 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_475 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_476:
    s -> svgapalettebase476 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_476 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_477:
    s -> svgapalettebase477 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_477 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_478:
    s -> svgapalettebase478 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_478 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_479:
    s -> svgapalettebase479 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_479 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_480:
    s -> svgapalettebase480 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_480 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_481:
    s -> svgapalettebase481 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_481 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_482:
    s -> svgapalettebase482 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_482 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_483:
    s -> svgapalettebase483 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_483 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_484:
    s -> svgapalettebase484 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_484 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_485:
    s -> svgapalettebase485 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_485 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_486:
    s -> svgapalettebase486 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_486 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_487:
    s -> svgapalettebase487 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_487 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_488:
    s -> svgapalettebase488 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_488 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_489:
    s -> svgapalettebase489 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_489 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_490:
    s -> svgapalettebase490 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_490 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_491:
    s -> svgapalettebase491 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_491 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_492:
    s -> svgapalettebase492 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_492 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_493:
    s -> svgapalettebase493 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_493 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_494:
    s -> svgapalettebase494 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_494 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_495:
    s -> svgapalettebase495 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_495 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_496:
    s -> svgapalettebase496 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_496 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_497:
    s -> svgapalettebase497 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_497 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_498:
    s -> svgapalettebase498 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_498 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_499:
    s -> svgapalettebase499 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_499 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_500:
    s -> svgapalettebase500 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_500 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_501:
    s -> svgapalettebase501 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_501 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_502:
    s -> svgapalettebase502 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_502 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_503:
    s -> svgapalettebase503 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_503 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_504:
    s -> svgapalettebase504 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_504 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_505:
    s -> svgapalettebase505 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_505 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_506:
    s -> svgapalettebase506 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_506 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_507:
    s -> svgapalettebase507 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_507 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_508:
    s -> svgapalettebase508 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_508 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_509:
    s -> svgapalettebase509 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_509 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_510:
    s -> svgapalettebase510 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_510 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_511:
    s -> svgapalettebase511 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_511 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_512:
    s -> svgapalettebase512 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_512 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_513:
    s -> svgapalettebase513 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_513 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_514:
    s -> svgapalettebase514 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_514 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_515:
    s -> svgapalettebase515 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_515 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_516:
    s -> svgapalettebase516 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_516 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_517:
    s -> svgapalettebase517 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_517 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_518:
    s -> svgapalettebase518 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_518 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_519:
    s -> svgapalettebase519 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_519 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_520:
    s -> svgapalettebase520 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_520 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_521:
    s -> svgapalettebase521 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_521 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_522:
    s -> svgapalettebase522 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_522 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_523:
    s -> svgapalettebase523 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_523 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_524:
    s -> svgapalettebase524 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_524 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_525:
    s -> svgapalettebase525 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_525 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_526:
    s -> svgapalettebase526 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_526 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_527:
    s -> svgapalettebase527 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_527 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_528:
    s -> svgapalettebase528 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_528 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_529:
    s -> svgapalettebase529 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_529 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_530:
    s -> svgapalettebase530 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_530 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_531:
    s -> svgapalettebase531 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_531 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_532:
    s -> svgapalettebase532 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_532 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_533:
    s -> svgapalettebase533 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_533 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_534:
    s -> svgapalettebase534 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_534 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_535:
    s -> svgapalettebase535 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_535 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_536:
    s -> svgapalettebase536 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_536 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_537:
    s -> svgapalettebase537 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_537 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_538:
    s -> svgapalettebase538 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_538 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_539:
    s -> svgapalettebase539 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_539 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_540:
    s -> svgapalettebase540 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_540 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_541:
    s -> svgapalettebase541 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_541 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_542:
    s -> svgapalettebase542 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_542 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_543:
    s -> svgapalettebase543 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_543 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_544:
    s -> svgapalettebase544 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_544 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_545:
    s -> svgapalettebase545 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_545 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_546:
    s -> svgapalettebase546 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_546 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_547:
    s -> svgapalettebase547 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_547 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_548:
    s -> svgapalettebase548 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_548 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_549:
    s -> svgapalettebase549 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_549 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_550:
    s -> svgapalettebase550 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_550 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_551:
    s -> svgapalettebase551 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_551 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_552:
    s -> svgapalettebase552 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_552 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_553:
    s -> svgapalettebase553 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_553 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_554:
    s -> svgapalettebase554 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_554 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_555:
    s -> svgapalettebase555 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_555 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_556:
    s -> svgapalettebase556 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_556 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_557:
    s -> svgapalettebase557 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_557 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_558:
    s -> svgapalettebase558 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_558 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_559:
    s -> svgapalettebase559 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_559 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_560:
    s -> svgapalettebase560 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_560 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_561:
    s -> svgapalettebase561 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_561 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_562:
    s -> svgapalettebase562 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_562 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_563:
    s -> svgapalettebase563 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_563 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_564:
    s -> svgapalettebase564 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_564 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_565:
    s -> svgapalettebase565 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_565 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_566:
    s -> svgapalettebase566 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_566 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_567:
    s -> svgapalettebase567 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_567 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_568:
    s -> svgapalettebase568 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_568 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_569:
    s -> svgapalettebase569 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_569 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_570:
    s -> svgapalettebase570 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_570 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_571:
    s -> svgapalettebase571 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_571 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_572:
    s -> svgapalettebase572 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_572 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_573:
    s -> svgapalettebase573 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_573 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_574:
    s -> svgapalettebase574 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_574 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_575:
    s -> svgapalettebase575 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_575 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_576:
    s -> svgapalettebase576 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_576 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_577:
    s -> svgapalettebase577 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_577 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_578:
    s -> svgapalettebase578 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_578 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_579:
    s -> svgapalettebase579 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_579 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_580:
    s -> svgapalettebase580 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_580 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_581:
    s -> svgapalettebase581 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_581 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_582:
    s -> svgapalettebase582 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_582 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_583:
    s -> svgapalettebase583 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_583 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_584:
    s -> svgapalettebase584 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_584 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_585:
    s -> svgapalettebase585 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_585 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_586:
    s -> svgapalettebase586 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_586 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_587:
    s -> svgapalettebase587 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_587 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_588:
    s -> svgapalettebase588 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_588 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_589:
    s -> svgapalettebase589 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_589 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_590:
    s -> svgapalettebase590 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_590 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_591:
    s -> svgapalettebase591 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_591 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_592:
    s -> svgapalettebase592 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_592 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_593:
    s -> svgapalettebase593 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_593 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_594:
    s -> svgapalettebase594 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_594 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_595:
    s -> svgapalettebase595 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_595 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_596:
    s -> svgapalettebase596 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_596 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_597:
    s -> svgapalettebase597 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_597 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_598:
    s -> svgapalettebase598 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_598 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_599:
    s -> svgapalettebase599 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_599 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_600:
    s -> svgapalettebase600 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_600 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_601:
    s -> svgapalettebase601 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_601 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_602:
    s -> svgapalettebase602 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_602 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_603:
    s -> svgapalettebase603 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_603 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_604:
    s -> svgapalettebase604 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_604 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_605:
    s -> svgapalettebase605 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_605 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_606:
    s -> svgapalettebase606 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_606 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_607:
    s -> svgapalettebase607 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_607 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_608:
    s -> svgapalettebase608 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_608 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_609:
    s -> svgapalettebase609 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_609 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_610:
    s -> svgapalettebase610 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_610 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_611:
    s -> svgapalettebase611 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_611 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_612:
    s -> svgapalettebase612 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_612 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_613:
    s -> svgapalettebase613 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_613 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_614:
    s -> svgapalettebase614 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_614 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_615:
    s -> svgapalettebase615 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_615 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_616:
    s -> svgapalettebase616 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_616 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_617:
    s -> svgapalettebase617 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_617 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_618:
    s -> svgapalettebase618 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_618 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_619:
    s -> svgapalettebase619 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_619 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_620:
    s -> svgapalettebase620 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_620 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_621:
    s -> svgapalettebase621 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_621 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_622:
    s -> svgapalettebase622 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_622 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_623:
    s -> svgapalettebase623 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_623 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_624:
    s -> svgapalettebase624 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_624 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_625:
    s -> svgapalettebase625 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_625 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_626:
    s -> svgapalettebase626 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_626 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_627:
    s -> svgapalettebase627 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_627 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_628:
    s -> svgapalettebase628 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_628 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_629:
    s -> svgapalettebase629 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_629 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_630:
    s -> svgapalettebase630 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_630 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_631:
    s -> svgapalettebase631 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_631 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_632:
    s -> svgapalettebase632 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_632 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_633:
    s -> svgapalettebase633 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_633 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_634:
    s -> svgapalettebase634 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_634 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_635:
    s -> svgapalettebase635 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_635 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_636:
    s -> svgapalettebase636 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_636 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_637:
    s -> svgapalettebase637 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_637 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_638:
    s -> svgapalettebase638 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_638 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_639:
    s -> svgapalettebase639 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_639 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_640:
    s -> svgapalettebase640 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_640 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_641:
    s -> svgapalettebase641 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_641 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_642:
    s -> svgapalettebase642 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_642 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_643:
    s -> svgapalettebase643 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_643 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_644:
    s -> svgapalettebase644 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_644 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_645:
    s -> svgapalettebase645 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_645 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_646:
    s -> svgapalettebase646 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_646 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_647:
    s -> svgapalettebase647 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_647 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_648:
    s -> svgapalettebase648 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_648 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_649:
    s -> svgapalettebase649 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_649 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_650:
    s -> svgapalettebase650 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_650 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_651:
    s -> svgapalettebase651 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_651 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_652:
    s -> svgapalettebase652 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_652 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_653:
    s -> svgapalettebase653 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_653 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_654:
    s -> svgapalettebase654 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_654 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_655:
    s -> svgapalettebase655 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_655 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_656:
    s -> svgapalettebase656 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_656 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_657:
    s -> svgapalettebase657 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_657 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_658:
    s -> svgapalettebase658 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_658 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_659:
    s -> svgapalettebase659 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_659 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_660:
    s -> svgapalettebase660 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_660 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_661:
    s -> svgapalettebase661 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_661 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_662:
    s -> svgapalettebase662 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_662 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_663:
    s -> svgapalettebase663 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_663 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_664:
    s -> svgapalettebase664 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_664 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_665:
    s -> svgapalettebase665 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_665 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_666:
    s -> svgapalettebase666 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_666 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_667:
    s -> svgapalettebase667 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_667 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_668:
    s -> svgapalettebase668 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_668 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_669:
    s -> svgapalettebase669 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_669 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_670:
    s -> svgapalettebase670 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_670 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_671:
    s -> svgapalettebase671 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_671 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_672:
    s -> svgapalettebase672 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_672 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_673:
    s -> svgapalettebase673 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_673 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_674:
    s -> svgapalettebase674 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_674 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_675:
    s -> svgapalettebase675 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_675 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_676:
    s -> svgapalettebase676 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_676 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_677:
    s -> svgapalettebase677 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_677 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_678:
    s -> svgapalettebase678 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_678 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_679:
    s -> svgapalettebase679 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_679 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_680:
    s -> svgapalettebase680 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_680 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_681:
    s -> svgapalettebase681 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_681 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_682:
    s -> svgapalettebase682 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_682 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_683:
    s -> svgapalettebase683 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_683 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_684:
    s -> svgapalettebase684 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_684 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_685:
    s -> svgapalettebase685 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_685 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_686:
    s -> svgapalettebase686 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_686 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_687:
    s -> svgapalettebase687 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_687 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_688:
    s -> svgapalettebase688 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_688 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_689:
    s -> svgapalettebase689 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_689 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_690:
    s -> svgapalettebase690 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_690 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_691:
    s -> svgapalettebase691 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_691 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_692:
    s -> svgapalettebase692 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_692 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_693:
    s -> svgapalettebase693 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_693 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_694:
    s -> svgapalettebase694 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_694 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_695:
    s -> svgapalettebase695 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_695 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_696:
    s -> svgapalettebase696 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_696 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_697:
    s -> svgapalettebase697 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_697 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_698:
    s -> svgapalettebase698 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_698 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_699:
    s -> svgapalettebase699 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_699 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_700:
    s -> svgapalettebase700 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_700 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_701:
    s -> svgapalettebase701 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_701 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_702:
    s -> svgapalettebase702 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_702 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_703:
    s -> svgapalettebase703 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_703 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_704:
    s -> svgapalettebase704 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_704 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_705:
    s -> svgapalettebase705 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_705 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_706:
    s -> svgapalettebase706 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_706 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_707:
    s -> svgapalettebase707 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_707 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_708:
    s -> svgapalettebase708 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_708 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_709:
    s -> svgapalettebase709 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_709 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_710:
    s -> svgapalettebase710 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_710 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_711:
    s -> svgapalettebase711 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_711 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_712:
    s -> svgapalettebase712 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_712 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_713:
    s -> svgapalettebase713 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_713 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_714:
    s -> svgapalettebase714 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_714 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_715:
    s -> svgapalettebase715 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_715 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_716:
    s -> svgapalettebase716 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_716 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_717:
    s -> svgapalettebase717 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_717 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_718:
    s -> svgapalettebase718 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_718 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_719:
    s -> svgapalettebase719 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_719 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_720:
    s -> svgapalettebase720 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_720 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_721:
    s -> svgapalettebase721 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_721 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_722:
    s -> svgapalettebase722 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_722 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_723:
    s -> svgapalettebase723 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_723 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_724:
    s -> svgapalettebase724 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_724 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_725:
    s -> svgapalettebase725 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_725 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_726:
    s -> svgapalettebase726 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_726 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_727:
    s -> svgapalettebase727 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_727 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_728:
    s -> svgapalettebase728 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_728 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_729:
    s -> svgapalettebase729 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_729 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_730:
    s -> svgapalettebase730 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_730 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_731:
    s -> svgapalettebase731 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_731 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_732:
    s -> svgapalettebase732 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_732 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_733:
    s -> svgapalettebase733 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_733 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_734:
    s -> svgapalettebase734 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_734 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_735:
    s -> svgapalettebase735 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_735 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_736:
    s -> svgapalettebase736 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_736 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_737:
    s -> svgapalettebase737 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_737 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_738:
    s -> svgapalettebase738 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_738 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_739:
    s -> svgapalettebase739 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_739 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_740:
    s -> svgapalettebase740 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_740 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_741:
    s -> svgapalettebase741 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_741 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_742:
    s -> svgapalettebase742 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_742 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_743:
    s -> svgapalettebase743 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_743 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_744:
    s -> svgapalettebase744 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_744 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_745:
    s -> svgapalettebase745 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_745 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_746:
    s -> svgapalettebase746 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_746 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_747:
    s -> svgapalettebase747 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_747 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_748:
    s -> svgapalettebase748 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_748 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_749:
    s -> svgapalettebase749 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_749 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_750:
    s -> svgapalettebase750 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_750 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_751:
    s -> svgapalettebase751 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_751 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_752:
    s -> svgapalettebase752 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_752 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_753:
    s -> svgapalettebase753 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_753 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_754:
    s -> svgapalettebase754 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_754 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_755:
    s -> svgapalettebase755 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_755 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_756:
    s -> svgapalettebase756 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_756 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_757:
    s -> svgapalettebase757 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_757 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_758:
    s -> svgapalettebase758 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_758 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_759:
    s -> svgapalettebase759 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_759 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_760:
    s -> svgapalettebase760 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_760 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_761:
    s -> svgapalettebase761 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_761 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_762:
    s -> svgapalettebase762 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_762 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_763:
    s -> svgapalettebase763 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_763 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_764:
    s -> svgapalettebase764 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_764 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_765:
    s -> svgapalettebase765 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_765 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_766:
    s -> svgapalettebase766 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_766 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_767:
    s -> svgapalettebase767 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_767 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_PALETTE_BASE_768:
    s -> svgapalettebase768 = value;
    #ifdef VERBOSE
    printf("%s: SVGA_PALETTE_BASE_768 register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  case SVGA_REG_DEV_CAP:
    if (value == 0) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 1) {
      s -> devcap_val = 0x00000008;
    };
    if (value == 2) {
      s -> devcap_val = 0x00000008;
    };
    if (value == 3) {
      s -> devcap_val = 0x00000008;
    };
    if (value == 4) {
      s -> devcap_val = 0x00000007;
    };
    if (value == 5) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 6) {
      s -> devcap_val = 0x0000000d;
    };
    if (value == 7) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 8) {
      s -> devcap_val = 0x00000008;
    };
    if (value == 9) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 10) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 11) {
      s -> devcap_val = 0x00000004;
    };
    if (value == 12) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 13) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 14) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 15) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 16) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 17) {
      s -> devcap_val = 0x000000bd;
    };
    if (value == 18) {
      s -> devcap_val = 0x00000014;
    };
    if (value == 19) {
      s -> devcap_val = 0x00008000;
    };
    if (value == 20) {
      s -> devcap_val = 0x00008000;
    };
    if (value == 21) {
      s -> devcap_val = 0x00004000;
    };
    if (value == 22) {
      s -> devcap_val = 0x00008000;
    };
    if (value == 23) {
      s -> devcap_val = 0x00008000;
    };
    if (value == 24) {
      s -> devcap_val = 0x00000010;
    };
    if (value == 25) {
      s -> devcap_val = 0x001fffff;
    };
    if (value == 26) {
      s -> devcap_val = 0x000fffff;
    };
    if (value == 27) {
      s -> devcap_val = 0x0000ffff;
    };
    if (value == 28) {
      s -> devcap_val = 0x0000ffff;
    };
    if (value == 29) {
      s -> devcap_val = 0x00000020;
    };
    if (value == 30) {
      s -> devcap_val = 0x00000020;
    };
    if (value == 31) {
      s -> devcap_val = 0x03ffffff;
    };
    if (value == 32) {
      s -> devcap_val = 0x0018ec1f;
    };
    if (value == 33) {
      s -> devcap_val = 0x0018e11f;
    };
    if (value == 34) {
      s -> devcap_val = 0x0008601f;
    };
    if (value == 35) {
      s -> devcap_val = 0x0008601f;
    };
    if (value == 36) {
      s -> devcap_val = 0x0008611f;
    };
    if (value == 37) {
      s -> devcap_val = 0x0000611f;
    };
    if (value == 38) {
      s -> devcap_val = 0x0018ec1f;
    };
    if (value == 39) {
      s -> devcap_val = 0x0000601f;
    };
    if (value == 40) {
      s -> devcap_val = 0x00006007;
    };
    if (value == 41) {
      s -> devcap_val = 0x0000601f;
    };
    if (value == 42) {
      s -> devcap_val = 0x0000601f;
    };
    if (value == 43) {
      s -> devcap_val = 0x000040c5;
    };
    if (value == 44) {
      s -> devcap_val = 0x000040c5;
    };
    if (value == 45) {
      s -> devcap_val = 0x000040c5;
    };
    if (value == 46) {
      s -> devcap_val = 0x0000e005;
    };
    if (value == 47) {
      s -> devcap_val = 0x0000e005;
    };
    if (value == 48) {
      s -> devcap_val = 0x0000e005;
    };
    if (value == 49) {
      s -> devcap_val = 0x0000e005;
    };
    if (value == 50) {
      s -> devcap_val = 0x0000e005;
    };
    if (value == 51) {
      s -> devcap_val = 0x00014005;
    };
    if (value == 52) {
      s -> devcap_val = 0x00014007;
    };
    if (value == 53) {
      s -> devcap_val = 0x00014007;
    };
    if (value == 54) {
      s -> devcap_val = 0x00014005;
    };
    if (value == 55) {
      s -> devcap_val = 0x00014001;
    };
    if (value == 56) {
      s -> devcap_val = 0x0080601f;
    };
    if (value == 57) {
      s -> devcap_val = 0x0080601f;
    };
    if (value == 58) {
      s -> devcap_val = 0x0080601f;
    };
    if (value == 59) {
      s -> devcap_val = 0x0080601f;
    };
    if (value == 60) {
      s -> devcap_val = 0x0080601f;
    };
    if (value == 61) {
      s -> devcap_val = 0x0080601f;
    };
    if (value == 62) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 63) {
      s -> devcap_val = 0x00000004;
    };
    if (value == 64) {
      s -> devcap_val = 0x00000008;
    };
    if (value == 65) {
      s -> devcap_val = 0x00014007;
    };
    if (value == 66) {
      s -> devcap_val = 0x0000601f;
    };
    if (value == 67) {
      s -> devcap_val = 0x0000601f;
    };
    if (value == 68) {
      s -> devcap_val = 0x01246000;
    };
    if (value == 69) {
      s -> devcap_val = 0x01246000;
    };
    if (value == 70) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 71) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 72) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 73) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 74) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 75) {
      s -> devcap_val = 0x01246000;
    };
    if (value == 76) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 77) {
      s -> devcap_val = 0x00000100;
    };
    if (value == 78) {
      s -> devcap_val = 0x00008000;
    };
    if (value == 79) {
      s -> devcap_val = 0x000040c5;
    };
    if (value == 80) {
      s -> devcap_val = 0x000040c5;
    };
    if (value == 81) {
      s -> devcap_val = 0x000040c5;
    };
    if (value == 82) {
      s -> devcap_val = 0x00006005;
    };
    if (value == 83) {
      s -> devcap_val = 0x00006005;
    };
    if (value == 84) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 85) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 86) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 87) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 88) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 89) {
      s -> devcap_val = 0x0000000a;
    };
    if (value == 90) {
      s -> devcap_val = 0x0000000a;
    };
    if (value == 91) {
      s -> devcap_val = 0x01246000;
    };
    if (value == 92) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 93) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 94) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 95) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 96) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 97) {
      s -> devcap_val = 0x00000010;
    };
    if (value == 98) {
      s -> devcap_val = 0x0000000f;
    };
    if (value == 99) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 100) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 101) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 102) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 103) {
      s -> devcap_val = 0x000000f7;
    };
    if (value == 104) {
      s -> devcap_val = 0x000000f7;
    };
    if (value == 105) {
      s -> devcap_val = 0x000000f7;
    };
    if (value == 106) {
      s -> devcap_val = 0x00000009;
    };
    if (value == 107) {
      s -> devcap_val = 0x0000026b;
    };
    if (value == 108) {
      s -> devcap_val = 0x0000026b;
    };
    if (value == 109) {
      s -> devcap_val = 0x0000000b;
    };
    if (value == 110) {
      s -> devcap_val = 0x000000f7;
    };
    if (value == 111) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 112) {
      s -> devcap_val = 0x000000f7;
    };
    if (value == 113) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 114) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 115) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 116) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 117) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 118) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 119) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 120) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 121) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 122) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 123) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 124) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 125) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 126) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 127) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 128) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 129) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 130) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 131) {
      s -> devcap_val = 0x000000f7;
    };
    if (value == 132) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 133) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 134) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 135) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 136) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 137) {
      s -> devcap_val = 0x0000026b;
    };
    if (value == 138) {
      s -> devcap_val = 0x000001e3;
    };
    if (value == 139) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 140) {
      s -> devcap_val = 0x000001f7;
    };
    if (value == 141) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 142) {
      s -> devcap_val = 0x00000041;
    };
    if (value == 143) {
      s -> devcap_val = 0x00000041;
    };
    if (value == 144) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 145) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 146) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 147) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 148) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 149) {
      s -> devcap_val = 0x000001e3;
    };
    if (value == 150) {
      s -> devcap_val = 0x000001e3;
    };
    if (value == 151) {
      s -> devcap_val = 0x000001e3;
    };
    if (value == 152) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 153) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 154) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 155) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 156) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 157) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 158) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 159) {
      s -> devcap_val = 0x00000261;
    };
    if (value == 160) {
      s -> devcap_val = 0x00000269;
    };
    if (value == 161) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 162) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 163) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 164) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 165) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 166) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 167) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 168) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 169) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 170) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 171) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 172) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 173) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 174) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 175) {
      s -> devcap_val = 0x00000269;
    };
    if (value == 176) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 177) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 178) {
      s -> devcap_val = 0x00000261;
    };
    if (value == 179) {
      s -> devcap_val = 0x00000269;
    };
    if (value == 180) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 181) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 182) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 183) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 184) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 185) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 186) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 187) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 188) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 189) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 190) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 191) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 192) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 193) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 194) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 195) {
      s -> devcap_val = 0x000003e7;
    };
    if (value == 196) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 197) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 198) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 199) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 200) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 201) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 202) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 203) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 204) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 205) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 206) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 207) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 208) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 209) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 210) {
      s -> devcap_val = 0x00000063;
    };
    if (value == 211) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 212) {
      s -> devcap_val = 0x00000045;
    };
    if (value == 213) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 214) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 215) {
      s -> devcap_val = 0x000002e1;
    };
    if (value == 216) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 217) {
      s -> devcap_val = 0x0000006b;
    };
    if (value == 218) {
      s -> devcap_val = 0x0000006b;
    };
    if (value == 219) {
      s -> devcap_val = 0x0000006b;
    };
    if (value == 220) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 221) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 222) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 223) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 224) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 225) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 226) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 227) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 228) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 229) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 230) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 231) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 232) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 233) {
      s -> devcap_val = 0x00000269;
    };
    if (value == 234) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 235) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 236) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 237) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 238) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 239) {
      s -> devcap_val = 0x000002f7;
    };
    if (value == 240) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 241) {
      s -> devcap_val = 0x000003f7;
    };
    if (value == 242) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 243) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 244) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 245) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 246) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 247) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 248) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 249) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 250) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 251) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 252) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 253) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 254) {
      s -> devcap_val = 0x000000e1;
    };
    if (value == 255) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 256) {
      s -> devcap_val = 0x000000e3;
    };
    if (value == 257) {
      s -> devcap_val = 0x00000000;
    };
    if (value == 258) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 259) {
      s -> devcap_val = 0x00000001;
    };
    if (value == 260) {
      s -> devcap_val = 0x00000010;
    };
    if (value == 261) {
      s -> devcap_val = 0x00000001;
    };
    if (value >= 262) {
      s -> devcap_val = 0x00000000;
    };
    #ifdef VERBOSE
    printf("%s: SVGA_REG_DEV_CAP register %u with the value of %u\n", __func__, s -> index, value);
    #endif
    break;
  default:
    #ifdef VERBOSE
    printf("%s: default register %u with the value of %u\n", __func__, s -> index, value);
    #endif
  }
}
static uint32_t vmsvga_irqstatus_read(void * opaque, uint32_t address) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_irqstatus_read was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  #ifdef VERBOSE
  printf("%s: vmsvga_irqstatus_read\n", __func__);
  #endif
  return s -> irq_status;
}
static void vmsvga_irqstatus_write(void * opaque, uint32_t address, uint32_t data) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_irqstatus_write was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  s -> irq_status &= ~data;
  #ifdef VERBOSE
  printf("%s: vmsvga_irqstatus_write %u\n", __func__, data);
  #endif
  struct pci_vmsvga_state_s * pci_vmsvga = container_of(s, struct pci_vmsvga_state_s, chip);
  PCIDevice * pci_dev = PCI_DEVICE(pci_vmsvga);
  if (!((s -> irq_mask & s -> irq_status))) {
    #ifdef VERBOSE
    printf("Pci_set_irq=O\n");
    #endif
    pci_set_irq(pci_dev, 0);
  }
}
static uint32_t vmsvga_bios_read(void * opaque, uint32_t address) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_bios_read was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  #ifdef VERBOSE
  printf("%s: vmsvga_bios_read\n", __func__);
  #endif
  return s -> bios;
}
static void vmsvga_bios_write(void * opaque, uint32_t address, uint32_t data) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_bios_write was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  s -> bios = data;
  #ifdef VERBOSE
  printf("%s: vmsvga_bios_write %u\n", __func__, data);
  #endif
}
static inline void vmsvga_check_size(struct vmsvga_state_s * s) {
  #ifdef VERBOSE
  //printf("vmsvga: vmsvga_check_size was just executed\n");
  #endif
  DisplaySurface * surface = qemu_console_surface(s -> vga.con);
  uint32_t new_stride;
  if (s -> pitchlock != 0) {
    new_stride = s -> pitchlock;
  } else {
    new_stride = (((s -> new_depth) * (s -> new_width)) / (8));
  }
  if (s -> new_width != surface_width(surface) || s -> new_height != surface_height(surface) || (new_stride != surface_stride(surface)) || s -> new_depth != surface_bits_per_pixel(surface)) {
    pixman_format_code_t format = qemu_default_pixman_format(s -> new_depth, true);
    surface = qemu_create_displaysurface_from(s -> new_width, s -> new_height, format, new_stride, s -> vga.vram_ptr);
    dpy_gfx_replace_surface(s -> vga.con, surface);
  }
}
static void vmsvga_update_display(void * opaque) {
  #ifdef VERBOSE
  //printf("vmsvga: vmsvga_update_display was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  if ((s -> enable >= 1 || s -> config >= 1) && (s -> new_width >= 1 && s -> new_height >= 1 && s -> new_depth >= 1)) {
    vmsvga_check_size(s);
    vmsvga_fifo_run(s);
    cursor_update_from_fifo(s);
    return;
  } else {
    s -> vga.hw_ops -> gfx_update( & s -> vga);
    return;
  }
  return;
}
static void vmsvga_reset(DeviceState * dev) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_reset was just executed\n");
  #endif
  struct pci_vmsvga_state_s * pci = VMWARE_SVGA(dev);
  struct vmsvga_state_s * s = & pci -> chip;
  s -> enable = 0;
  s -> config = 0;
}
static void vmsvga_invalidate_display(void * opaque) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_invalidate_display was just executed\n");
  #endif
}
static void vmsvga_text_update(void * opaque, console_ch_t * chardata) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_text_update was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  if (s -> vga.hw_ops -> text_update) {
    s -> vga.hw_ops -> text_update( & s -> vga, chardata);
  }
}
static int vmsvga_post_load(void * opaque, int version_id) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_post_load was just executed\n");
  #endif
  struct vmsvga_state_s *s = opaque;
  s -> enable = 1;
  s -> config = 1;
  return 0;
}
static
const VMStateDescription vmstate_vmware_vga_internal = {
  .name = "vmware_vga_internal",
  .version_id = 1,
  .minimum_version_id = 0,
  .post_load = vmsvga_post_load,
  .fields = (VMStateField[]) {
    VMSTATE_UINT32(svgapalettebase0, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase1, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase2, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase3, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase4, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase5, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase6, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase7, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase8, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase9, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase10, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase11, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase12, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase13, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase14, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase15, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase16, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase17, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase18, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase19, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase20, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase21, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase22, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase23, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase24, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase25, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase26, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase27, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase28, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase29, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase30, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase31, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase32, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase33, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase34, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase35, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase36, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase37, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase38, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase39, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase40, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase41, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase42, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase43, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase44, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase45, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase46, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase47, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase48, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase49, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase50, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase51, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase52, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase53, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase54, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase55, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase56, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase57, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase58, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase59, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase60, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase61, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase62, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase63, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase64, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase65, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase66, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase67, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase68, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase69, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase70, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase71, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase72, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase73, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase74, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase75, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase76, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase77, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase78, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase79, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase80, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase81, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase82, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase83, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase84, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase85, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase86, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase87, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase88, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase89, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase90, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase91, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase92, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase93, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase94, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase95, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase96, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase97, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase98, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase99, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase100, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase101, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase102, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase103, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase104, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase105, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase106, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase107, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase108, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase109, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase110, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase111, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase112, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase113, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase114, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase115, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase116, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase117, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase118, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase119, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase120, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase121, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase122, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase123, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase124, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase125, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase126, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase127, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase128, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase129, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase130, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase131, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase132, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase133, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase134, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase135, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase136, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase137, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase138, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase139, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase140, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase141, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase142, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase143, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase144, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase145, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase146, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase147, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase148, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase149, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase150, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase151, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase152, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase153, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase154, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase155, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase156, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase157, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase158, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase159, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase160, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase161, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase162, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase163, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase164, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase165, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase166, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase167, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase168, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase169, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase170, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase171, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase172, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase173, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase174, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase175, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase176, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase177, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase178, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase179, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase180, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase181, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase182, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase183, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase184, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase185, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase186, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase187, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase188, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase189, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase190, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase191, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase192, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase193, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase194, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase195, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase196, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase197, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase198, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase199, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase200, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase201, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase202, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase203, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase204, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase205, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase206, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase207, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase208, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase209, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase210, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase211, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase212, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase213, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase214, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase215, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase216, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase217, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase218, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase219, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase220, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase221, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase222, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase223, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase224, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase225, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase226, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase227, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase228, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase229, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase230, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase231, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase232, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase233, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase234, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase235, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase236, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase237, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase238, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase239, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase240, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase241, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase242, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase243, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase244, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase245, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase246, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase247, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase248, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase249, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase250, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase251, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase252, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase253, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase254, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase255, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase256, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase257, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase258, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase259, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase260, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase261, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase262, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase263, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase264, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase265, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase266, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase267, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase268, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase269, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase270, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase271, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase272, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase273, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase274, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase275, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase276, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase277, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase278, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase279, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase280, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase281, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase282, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase283, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase284, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase285, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase286, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase287, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase288, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase289, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase290, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase291, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase292, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase293, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase294, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase295, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase296, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase297, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase298, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase299, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase300, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase301, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase302, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase303, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase304, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase305, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase306, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase307, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase308, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase309, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase310, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase311, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase312, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase313, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase314, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase315, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase316, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase317, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase318, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase319, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase320, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase321, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase322, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase323, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase324, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase325, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase326, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase327, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase328, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase329, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase330, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase331, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase332, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase333, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase334, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase335, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase336, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase337, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase338, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase339, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase340, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase341, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase342, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase343, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase344, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase345, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase346, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase347, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase348, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase349, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase350, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase351, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase352, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase353, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase354, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase355, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase356, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase357, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase358, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase359, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase360, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase361, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase362, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase363, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase364, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase365, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase366, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase367, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase368, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase369, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase370, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase371, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase372, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase373, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase374, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase375, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase376, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase377, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase378, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase379, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase380, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase381, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase382, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase383, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase384, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase385, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase386, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase387, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase388, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase389, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase390, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase391, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase392, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase393, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase394, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase395, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase396, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase397, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase398, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase399, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase400, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase401, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase402, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase403, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase404, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase405, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase406, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase407, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase408, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase409, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase410, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase411, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase412, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase413, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase414, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase415, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase416, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase417, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase418, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase419, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase420, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase421, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase422, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase423, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase424, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase425, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase426, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase427, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase428, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase429, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase430, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase431, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase432, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase433, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase434, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase435, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase436, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase437, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase438, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase439, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase440, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase441, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase442, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase443, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase444, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase445, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase446, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase447, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase448, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase449, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase450, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase451, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase452, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase453, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase454, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase455, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase456, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase457, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase458, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase459, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase460, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase461, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase462, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase463, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase464, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase465, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase466, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase467, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase468, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase469, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase470, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase471, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase472, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase473, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase474, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase475, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase476, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase477, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase478, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase479, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase480, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase481, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase482, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase483, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase484, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase485, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase486, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase487, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase488, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase489, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase490, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase491, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase492, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase493, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase494, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase495, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase496, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase497, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase498, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase499, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase500, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase501, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase502, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase503, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase504, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase505, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase506, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase507, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase508, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase509, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase510, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase511, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase512, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase513, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase514, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase515, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase516, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase517, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase518, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase519, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase520, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase521, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase522, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase523, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase524, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase525, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase526, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase527, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase528, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase529, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase530, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase531, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase532, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase533, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase534, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase535, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase536, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase537, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase538, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase539, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase540, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase541, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase542, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase543, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase544, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase545, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase546, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase547, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase548, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase549, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase550, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase551, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase552, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase553, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase554, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase555, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase556, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase557, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase558, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase559, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase560, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase561, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase562, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase563, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase564, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase565, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase566, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase567, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase568, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase569, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase570, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase571, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase572, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase573, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase574, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase575, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase576, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase577, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase578, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase579, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase580, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase581, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase582, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase583, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase584, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase585, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase586, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase587, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase588, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase589, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase590, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase591, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase592, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase593, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase594, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase595, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase596, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase597, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase598, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase599, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase600, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase601, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase602, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase603, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase604, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase605, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase606, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase607, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase608, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase609, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase610, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase611, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase612, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase613, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase614, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase615, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase616, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase617, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase618, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase619, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase620, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase621, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase622, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase623, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase624, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase625, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase626, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase627, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase628, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase629, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase630, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase631, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase632, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase633, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase634, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase635, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase636, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase637, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase638, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase639, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase640, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase641, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase642, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase643, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase644, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase645, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase646, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase647, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase648, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase649, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase650, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase651, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase652, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase653, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase654, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase655, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase656, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase657, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase658, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase659, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase660, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase661, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase662, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase663, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase664, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase665, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase666, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase667, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase668, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase669, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase670, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase671, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase672, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase673, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase674, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase675, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase676, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase677, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase678, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase679, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase680, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase681, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase682, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase683, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase684, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase685, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase686, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase687, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase688, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase689, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase690, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase691, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase692, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase693, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase694, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase695, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase696, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase697, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase698, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase699, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase700, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase701, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase702, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase703, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase704, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase705, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase706, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase707, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase708, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase709, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase710, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase711, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase712, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase713, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase714, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase715, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase716, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase717, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase718, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase719, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase720, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase721, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase722, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase723, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase724, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase725, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase726, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase727, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase728, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase729, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase730, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase731, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase732, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase733, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase734, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase735, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase736, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase737, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase738, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase739, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase740, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase741, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase742, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase743, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase744, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase745, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase746, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase747, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase748, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase749, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase750, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase751, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase752, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase753, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase754, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase755, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase756, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase757, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase758, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase759, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase760, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase761, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase762, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase763, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase764, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase765, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase766, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase767, struct vmsvga_state_s),
    VMSTATE_UINT32(svgapalettebase768, struct vmsvga_state_s),
    VMSTATE_UINT32(enable, struct vmsvga_state_s),
    VMSTATE_UINT32(config, struct vmsvga_state_s),
    VMSTATE_UINT32(index, struct vmsvga_state_s),
    VMSTATE_UINT32(scratch_size, struct vmsvga_state_s),
    VMSTATE_UINT32(new_width, struct vmsvga_state_s),
    VMSTATE_UINT32(new_height, struct vmsvga_state_s),
    VMSTATE_UINT32(new_depth, struct vmsvga_state_s),
    VMSTATE_UINT32(num_gd, struct vmsvga_state_s),
    VMSTATE_UINT32(disp_prim, struct vmsvga_state_s),
    VMSTATE_UINT32(disp_x, struct vmsvga_state_s),
    VMSTATE_UINT32(disp_y, struct vmsvga_state_s),
    VMSTATE_UINT32(devcap_val, struct vmsvga_state_s),
    VMSTATE_UINT32(gmrdesc, struct vmsvga_state_s),
    VMSTATE_UINT32(gmrid, struct vmsvga_state_s),
    VMSTATE_UINT32(gmrpage, struct vmsvga_state_s),
    VMSTATE_UINT32(tracez, struct vmsvga_state_s),
    VMSTATE_UINT32(cmd_low, struct vmsvga_state_s),
    VMSTATE_UINT32(cmd_high, struct vmsvga_state_s),
    VMSTATE_UINT32(guest, struct vmsvga_state_s),
    VMSTATE_UINT32(svgaid, struct vmsvga_state_s),
    VMSTATE_UINT32(thread, struct vmsvga_state_s),
    VMSTATE_UINT32(sync, struct vmsvga_state_s),
    VMSTATE_UINT32(bios, struct vmsvga_state_s),
    VMSTATE_UINT32(syncing, struct vmsvga_state_s),
    VMSTATE_UINT32(fifo_size, struct vmsvga_state_s),
    VMSTATE_UINT32(fifo_min, struct vmsvga_state_s),
    VMSTATE_UINT32(fifo_max, struct vmsvga_state_s),
    VMSTATE_UINT32(fifo_next, struct vmsvga_state_s),
    VMSTATE_UINT32(fifo_stop, struct vmsvga_state_s),
    VMSTATE_UINT32(irq_mask, struct vmsvga_state_s),
    VMSTATE_UINT32(irq_status, struct vmsvga_state_s),
    VMSTATE_UINT32(display_id, struct vmsvga_state_s),
    VMSTATE_UINT32(pitchlock, struct vmsvga_state_s),
    VMSTATE_UINT32(cursor, struct vmsvga_state_s),
    VMSTATE_END_OF_LIST()
  }
};
static
const VMStateDescription vmstate_vmware_vga = {
  .name = "vmware_vga",
  .version_id = 0,
  .minimum_version_id = 0,
  .fields = (VMStateField[]) {
    VMSTATE_PCI_DEVICE(parent_obj, struct pci_vmsvga_state_s),
    VMSTATE_STRUCT(chip, struct pci_vmsvga_state_s, 0, vmstate_vmware_vga_internal, struct vmsvga_state_s),
    VMSTATE_END_OF_LIST()
  }
};
static
const GraphicHwOps vmsvga_ops = {
  .invalidate = vmsvga_invalidate_display,
  .gfx_update = vmsvga_update_display,
  .text_update = vmsvga_text_update,
};
static void vmsvga_init(DeviceState * dev, struct vmsvga_state_s * s,
  MemoryRegion * address_space, MemoryRegion * io) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_init was just executed\n");
  #endif
  s -> scratch_size = 64;
  s -> scratch = g_malloc(s -> scratch_size * 4);
  s -> vga.con = graphic_console_init(dev, 0, & vmsvga_ops, s);
  s -> fifo_size = 262144;
  memory_region_init_ram( & s -> fifo_ram, NULL, "vmsvga.fifo", s -> fifo_size, & error_fatal);
  s -> fifo = (uint32_t * ) memory_region_get_ram_ptr( & s -> fifo_ram);
  vga_common_init( & s -> vga, OBJECT(dev), & error_fatal);
  vga_init( & s -> vga, OBJECT(dev), address_space, io, true);
  vmstate_register(NULL, 0, & vmstate_vga_common, & s -> vga);
  if (s -> thread <= 0) {
    s -> thread++;
    s -> new_width = 1024;
    s -> new_height = 768;
    s -> new_depth = 32;
    pthread_t threads[1];
    pthread_create(threads, NULL, vmsvga_loop, (void * ) s);
  };
}
static uint64_t vmsvga_io_read(void * opaque, hwaddr addr, unsigned size) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_io_read was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  switch (addr) {
  case 1 * SVGA_INDEX_PORT:
    #ifdef VERBOSE
    printf("vmsvga: vmsvga_io_read SVGA_INDEX_PORT\n");
    #endif
    return vmsvga_index_read(s, addr);
  case 1 * SVGA_VALUE_PORT:
    #ifdef VERBOSE
    printf("vmsvga: vmsvga_io_read SVGA_VALUE_PORT\n");
    #endif
    return vmsvga_value_read(s, addr);
  case 1 * SVGA_BIOS_PORT:
    #ifdef VERBOSE
    printf("vmsvga: vmsvga_io_read SVGA_BIOS_PORT\n");
    #endif
    return vmsvga_bios_read(s, addr);
  case 1 * SVGA_IRQSTATUS_PORT:
    #ifdef VERBOSE
    printf("vmsvga: vmsvga_io_read SVGA_IRQSTATUS_PORT\n");
    #endif
    return vmsvga_irqstatus_read(s, addr);
  default:
    #ifdef VERBOSE
    printf("vmsvga: vmsvga_io_read default\n");
    #endif
    return 0;
  }
}
static void vmsvga_io_write(void * opaque, hwaddr addr,
  uint64_t data, unsigned size) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_io_write was just executed\n");
  #endif
  struct vmsvga_state_s * s = opaque;
  switch (addr) {
  case 1 * SVGA_INDEX_PORT:
    #ifdef VERBOSE
    printf("vmsvga: vmsvga_io_write SVGA_INDEX_PORT\n");
    #endif
    vmsvga_index_write(s, addr, data);
    break;
  case 1 * SVGA_VALUE_PORT:
    #ifdef VERBOSE
    printf("vmsvga: vmsvga_io_write SVGA_VALUE_PORT\n");
    #endif
    vmsvga_value_write(s, addr, data);
    break;
  case 1 * SVGA_BIOS_PORT:
    #ifdef VERBOSE
    printf("vmsvga: vmsvga_io_write SVGA_BIOS_PORT\n");
    #endif
    vmsvga_bios_write(s, addr, data);
    break;
  case 1 * SVGA_IRQSTATUS_PORT:
    #ifdef VERBOSE
    printf("vmsvga: vmsvga_io_write SVGA_IRQSTATUS_PORT\n");
    #endif
    vmsvga_irqstatus_write(s, addr, data);
    break;
  default:
    #ifdef VERBOSE
    printf("vmsvga: vmsvga_io_write default\n");
    #endif
    break;
  }
}
static
const MemoryRegionOps vmsvga_io_ops = {
  .read = vmsvga_io_read,
  .write = vmsvga_io_write,
  .endianness = DEVICE_LITTLE_ENDIAN,
  .valid = {
    .min_access_size = 4,
    .max_access_size = 4,
    .unaligned = true,
  },
  .impl = {
    .unaligned = true,
  },
};
static void pci_vmsvga_realize(PCIDevice * dev, Error ** errp) {
  #ifdef VERBOSE
  printf("vmsvga: pci_vmsvga_realize was just executed\n");
  #endif
  struct pci_vmsvga_state_s * s = VMWARE_SVGA(dev);
  dev -> config[PCI_CACHE_LINE_SIZE] = 0x08;
  dev -> config[PCI_LATENCY_TIMER] = 0x40;
  dev -> config[PCI_INTERRUPT_LINE] = 0xff;
  dev -> config[PCI_INTERRUPT_PIN] = 1;
  memory_region_init_io( & s -> io_bar, OBJECT(dev), & vmsvga_io_ops, & s -> chip, "vmsvga-io", 0x10);
  memory_region_set_flush_coalesced( & s -> io_bar);
  pci_register_bar(dev, 0, PCI_BASE_ADDRESS_SPACE_IO, & s -> io_bar);
  vmsvga_init(DEVICE(dev), & s -> chip, pci_address_space(dev), pci_address_space_io(dev));
  pci_register_bar(dev, 1, PCI_BASE_ADDRESS_MEM_TYPE_32, & s -> chip.vga.vram);
  pci_register_bar(dev, 2, PCI_BASE_ADDRESS_MEM_PREFETCH, & s -> chip.fifo_ram);
}
static Property vga_vmware_properties[] = {
  DEFINE_PROP_UINT32("vgamem_mb", struct pci_vmsvga_state_s,
    chip.vga.vram_size_mb, 128),
  DEFINE_PROP_BOOL("global-vmstate", struct pci_vmsvga_state_s,
    chip.vga.global_vmstate, true),
  DEFINE_PROP_END_OF_LIST(),
};
static void vmsvga_class_init(ObjectClass * klass, void * data) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_class_init was just executed\n");
  #endif
  DeviceClass * dc = DEVICE_CLASS(klass);
  PCIDeviceClass * k = PCI_DEVICE_CLASS(klass);
  k -> realize = pci_vmsvga_realize;
  k -> romfile = "vgabios-vmware.bin";
  k -> vendor_id = PCI_VENDOR_ID_VMWARE;
  k -> device_id = PCI_DEVICE_ID_VMWARE_SVGA2;
  k -> class_id = PCI_CLASS_DISPLAY_VGA;
  k -> subsystem_vendor_id = PCI_VENDOR_ID_VMWARE;
  k -> subsystem_id = PCI_DEVICE_ID_VMWARE_SVGA2;
  k -> revision = 0x00;
  dc -> reset = vmsvga_reset;
  dc -> vmsd = & vmstate_vmware_vga;
  device_class_set_props(dc, vga_vmware_properties);
  dc -> hotpluggable = false;
  set_bit(DEVICE_CATEGORY_DISPLAY, dc -> categories);
}
static
const TypeInfo vmsvga_info = {
  .name = "vmware-svga",
  .parent = TYPE_PCI_DEVICE,
  .instance_size = sizeof(struct pci_vmsvga_state_s),
  .class_init = vmsvga_class_init,
  .interfaces = (InterfaceInfo[]) {
    {
      INTERFACE_CONVENTIONAL_PCI_DEVICE
    }, {},
  },
};
static void vmsvga_register_types(void) {
  #ifdef VERBOSE
  printf("vmsvga: vmsvga_register_types was just executed\n");
  #endif
  type_register_static( & vmsvga_info);
}
type_init(vmsvga_register_types)
