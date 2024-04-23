import com.android.build.api.dsl.Packaging

plugins {
    alias(libs.plugins.androidLibrary)
}



android {
    namespace = "com.geehe.videonative"
    compileSdk = 34

    defaultConfig {
        minSdk = 26
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        consumerProguardFiles("consumer-rules.pro")
        ndk {
            debugSymbolLevel = "FULL"
        }
//        externalNativeBuild {
//            cmake {
//                cppFlags "-std=c++17"
//                arguments "-DANDROID_STL=c++_shared"
//            }
//        }

    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    externalNativeBuild {
        cmake {
            path("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    ndkVersion = "21.0.6113669"
//    packaging {
//        jniLibs {
//            excludes += "**.so"
//        }
//    }

//    buildTypes {
//        debug {
//            manifestPlaceholders["usesCleartextTraffic"] = "true"
//            isJniDebuggable = true
//            isMinifyEnabled = false
//            packaging {
//                jniLibs.keepDebugSymbols.add("*/**/*.so")
//            }
//        }
//    }


//    ndk {
//        debugSymbolLevel 'FULL'
//    }
}

dependencies {
    implementation(libs.appcompat)
    implementation(libs.material)
    implementation(libs.constraintlayout)

    testImplementation(libs.junit)
    androidTestImplementation(libs.ext.junit)

    androidTestImplementation(libs.espresso.core)
}