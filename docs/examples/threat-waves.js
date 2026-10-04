// Factory Tour example plugin: sends a wave of Scrap crawlers at the factory every month and pays a bounty for
// each one a turret destroys. Uses the `factory` API (plugin API 135) and the factory hooks.
registerPlugin({
    name: 'factory-threat-waves',
    version: '1.0',
    authors: ['Factory Tour contributors'],
    type: 'remote',
    licence: 'GPL-3.0',
    targetApiVersion: 135,
    main: function () {
        var crawler = 'factory-tour.factory_prototype.scrap_crawler';
        var wave = 1;

        context.subscribe('interval.day', function () {
            if (date.day !== 1 || factory.machines.length === 0) {
                return;
            }
            // Spawn along the map's west edge, a little more each month.
            for (var i = 0; i < wave; i++) {
                factory.spawnThreat(crawler, 2 * 32 + 16, (4 + i * 3) * 32 + 16);
            }
            park.postMessage({ type: 'blank', text: 'Wave ' + wave + ': ' + wave + ' crawlers approach the factory!' });
            wave++;
        });

        context.subscribe('factory.damage', function (e) {
            if (e.target === 'threat' && e.destroyed && e.damageType === 1) {
                // damageType 1 is turret fire; scripts choose their own numbers for other sources.
                console.log('Turret destroyed crawler ' + e.id);
            }
        });
    }
});
