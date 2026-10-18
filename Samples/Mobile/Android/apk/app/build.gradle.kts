plugins {
    id("com.android.application")
}

val vaseRoot = rootProject.projectDir.resolve("../../../..").canonicalFile
val androidBuild = vaseRoot.resolve("build-android/android-arm64-clang-release")
val apkInputs = androidBuild.resolve("apk-inputs")

android {
    namespace = "com.moozen.vase.mobile"
    compileSdk = 37
    ndkVersion = "30.0.16248370"

    defaultConfig {
        applicationId = "com.moozen.vase.mobile"
        minSdk = 29
        targetSdk = 37
        versionCode = 1
        versionName = "0.1"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    sourceSets.getByName("main") {
        assets.srcDir(apkInputs.resolve("assets"))
        jniLibs.srcDir(apkInputs.resolve("jniLibs"))
    }
}

dependencies {
    androidTestImplementation("androidx.test:runner:1.7.0")
    androidTestImplementation("androidx.test.ext:junit:1.3.0")
}

val prepareVaseMobileInputs by tasks.registering(Exec::class) {
    group = "build"
    description = "Generate and verify the existing NDK JNI/artifacts before APK packaging"
    workingDir = vaseRoot
    commandLine("cmake", "--build", androidBuild.absolutePath, "--target", "VaseMobileApkInputs")
}

tasks.named("preBuild") {
    dependsOn(prepareVaseMobileInputs)
}
