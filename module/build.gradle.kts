import android.databinding.tool.ext.capitalizeUS
import org.ajoberstar.grgit.Grgit

plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.grgit)
}
android{

    buildFeatures {
        resValues = false
        buildConfig = false

    }
}
var git = Grgit.open{  dir = rootProject.rootDir  } // 明确指定目录


val moduleId by extra("zygiskADI")
val moduleName by extra("zygiskADI")
val verName by extra("v0-0.2")
val verCode by extra(git.log().size)
val commitHash by extra(git.head().abbreviatedId)


androidComponents.onVariants { variant ->
    val variantLowered = variant.name.lowercase()
    val variantCapped = variant.name.capitalizeUS()
    val buildTypeLowered = variant.buildType?.lowercase()
    println("$buildTypeLowered")
    val moduleDir = layout.buildDirectory.dir("outputs/module/$variantLowered")
    val zipFileName = "$moduleId-$verName-$verCode-$commitHash-$buildTypeLowered.zip".replace(' ', '-')
    val prepareModuleFilesTask = task<Sync>("prepareModuleFiles$variantCapped") {
        println("$buildTypeLowered")
        dependsOn(
            ":ADI:externalNativeBuild$variantCapped",
            ":Zygisk:externalNativeBuild$variantCapped",
            ":ADILib:externalNativeBuild$variantCapped",
        )
        into(moduleDir)
        from("$projectDir/src") {
            exclude("module.prop")
//            filter<FixCrLfFilter>("eol" to FixCrLfFilter.CrLf.newInstance("lf"))
        }
        from("$projectDir/src") {
            include("module.prop")
            expand(
                "moduleId" to moduleId,
                "moduleName" to moduleName,
                "versionName" to "$verName($variantLowered)",
                "versionCode" to verCode
            )
        }

        into("lib/arm64-v8a"){
            from(project(":Zygisk").layout.buildDirectory.file("intermediates/cmake/$variantLowered/obj/arm64-v8a/libzygisk.so"))
            from(project(":ADILib").layout.buildDirectory.file("intermediates/cmake/$variantLowered/obj/arm64-v8a/libDrmHook.so"))
        }
        into("bin"){
            from(project(":Zygisk").layout.buildDirectory.file("intermediates/cmake/$variantLowered/obj/arm64-v8a/zygiskd"))
            from(project(":Zygisk").layout.buildDirectory.file("intermediates/cmake/$variantLowered/obj/arm64-v8a/zygiskc"))
            from(project(":ADI").layout.buildDirectory.file("intermediates/cmake/$variantLowered/obj/arm64-v8a/adi"))

        }


    }

    val zipTask = task<Zip>("zip$variantCapped") {
        group = "module"
        dependsOn(prepareModuleFilesTask)
        archiveFileName.set(zipFileName)
        destinationDirectory.set(layout.buildDirectory.file("outputs/release").get().asFile)
        from(moduleDir)
    }

    val pushTask = task<Exec>("push$variantCapped") {
        group = "module"
        dependsOn(zipTask)
        commandLine("adb", "push", zipTask.outputs.files.singleFile.path, "/data/local/tmp")
    }


    val installKsuTask = task<Exec>("installKsu$variantCapped") {
        group = "module"
        dependsOn(pushTask)
        commandLine(
            "adb", "shell", "su", "-c",
            "/data/adb/ksud module install /data/local/tmp/$zipFileName"
        )
    }

    val installMagiskTask = task<Exec>("installMagisk$variantCapped") {
        group = "module"
        dependsOn(pushTask)
        commandLine(
            "adb",
            "shell",
            "su",
            "-M",
            "-c",
            "magisk --install-module /data/local/tmp/$zipFileName"
        )
    }

    task<Exec>("installKsuAndReboot$variantCapped") {
        group = "module"
        dependsOn(installKsuTask)
        commandLine("adb", "reboot")
    }

    task<Exec>("installMagiskAndReboot$variantCapped") {
        group = "module"
        dependsOn(installMagiskTask)
        commandLine("adb", "reboot")
    }


}

task("logSomething2") {
    println("这是从 Gradle Task 打印的日志信息")
}
tasks.register("logSomething") {
    doLast {
        println("这是从 Gradle Task 打印的日志信息")
    }
}
