export type ExtraKey = 'FBX-glTF-conv';

export interface DocumentExtra {
    animationFrameRate?: number;

    /**
     * Requires command line option: `--export-fbx-file-header-info`.
     */
    fbxFileHeaderInfo?: {
        /**
         * Corresponding to `FbxIOFileHeaderInfo::mCreator`.
         */
        creator: string;

        /**
         * If unspecified, indicates `FbxIOFileHeaderInfo::mCreationTimeStampPresent` is false.
         * Otherwise, corresponding to `FbxIOFileHeaderInfo::mCreationTimeStamp`.
         */
        creationTimeStamp?: {
            /**
             * Corresponding to `FbxLocalTime::mYear`.
             */
            year: number;
            /**
             * Corresponding to `FbxLocalTime::mMonth`.
             */
            month: number;
            /**
             * Corresponding to `FbxLocalTime::mDay`.
             */
            day: number;
            /**
             * Corresponding to `FbxLocalTime::mHour`.
             */
            hour: number;
            /**
             * Corresponding to `FbxLocalTime::mMinute`.
             */
            minute: number;
            /**
             * Corresponding to `FbxLocalTime::mSecond`.
             */
            second: number;
            /**
             * Corresponding to `FbxLocalTime::mMillisecond`.
             */
            millisecond: number;
        };

        sceneInfo: FbxSceneInfo;
    };
}

export interface FbxSceneInfo {
    url: string;
    original: {
        applicationVendor: string;
        applicationName: string;
        applicationVersion: string;
        fileName: string;
    };
    title: string;
    subject: string;
    author: string;
    keywords: string;
    revision: string;
    comment: string;
}

export interface MaterialExtra {
    raw?: {
        type: 'lambert';
        properties: FbxSurfaceLambertProperties;
    } | {
        type: 'phong';
        properties: FbxSurfacePhongProperties;
    } | {
        properties: unknown;
    };
}

export interface FbxSurfaceMaterialProperties {
    shadingModel: string;
}

export interface FbxSurfaceLambertProperties extends FbxSurfaceMaterialProperties {
    emissive: FbxMaterialProperty<FbxDouble3>;
    emissiveFactor: FbxMaterialProperty<FbxDouble>;
    ambient: FbxMaterialProperty<FbxDouble3>;
    ambientFactor: FbxMaterialProperty<FbxDouble>;
    diffuse: FbxMaterialProperty<FbxDouble3>;
    diffuseFactor: FbxMaterialProperty<FbxDouble>;
    normalMap: FbxMaterialProperty<FbxDouble3>;
    bump: FbxMaterialProperty<FbxDouble3>;
    bumpFactor: FbxMaterialProperty<FbxDouble>;
    transparentColor: FbxMaterialProperty<FbxDouble3>;
    transparencyFactor: FbxMaterialProperty<FbxDouble>;
    displacementColor: FbxMaterialProperty<FbxDouble3>;
    displacementFactor: FbxMaterialProperty<FbxDouble>;
    vectorDisplacementColor: FbxMaterialProperty<FbxDouble3>;
    vectorDisplacementFactor: FbxMaterialProperty<FbxDouble>;
}

export interface FbxSurfacePhongProperties extends FbxSurfaceLambertProperties {
    specular: FbxMaterialProperty<FbxDouble3>;
    specularFactor: FbxMaterialProperty<FbxDouble>;
    shininess: FbxMaterialProperty<FbxDouble>;
    reflection: FbxMaterialProperty<FbxDouble3>;
    reflectionFactor: FbxMaterialProperty<FbxDouble>;
}

export type FbxDouble = number;

export type FbxDouble3 = [number, number, number];

export type FbxMaterialProperty<T> = {
    value: T;
    texture?: TextureReference;
};

export type TextureReference = {
    /**
     * Index to the glTF texture array.
     */
    index: number;
};
