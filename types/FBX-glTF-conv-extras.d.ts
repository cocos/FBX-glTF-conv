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
    };
}
